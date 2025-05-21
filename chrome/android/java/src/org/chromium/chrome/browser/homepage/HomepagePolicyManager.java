// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceUtil;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrar.PrefObserver;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.url.GURL;

/**
 * Provides information for the home page related policies.
 * Monitors changes for the homepage preference.
 */
public class HomepagePolicyManager implements PrefObserver {
    /** An interface to receive updates from {@link HomepagePolicyManager}. */
    public interface HomepagePolicyStateListener {
        /**
         * Will be called when homepage policy status change. Though cases are rare, when homepage
         * policy has changed during runtime, listeners will receive updates.
         */
        void onHomepagePolicyUpdate();
    }

    private static HomepagePolicyManager sInstance;

    private static PrefService sPrefServiceForTesting;

    private boolean mIsHomepageLocationManaged;
    @NonNull private GURL mHomepageUrl;

    private boolean mIsHomeButtonManaged;
    private boolean mHomeButtonPolicyValue;

    private boolean mIsInitializedWithNative;
    private PrefChangeRegistrar mPrefChangeRegistrar;

    private final SharedPreferencesManager mSharedPreferenceManager;
    private final ObserverList<HomepagePolicyStateListener> mListeners = new ObserverList<>();

    /**
     * @return The singleton instance of {@link HomepagePolicyManager}.
     */
    static HomepagePolicyManager getInstance() {
        if (sInstance == null) {
            sInstance = new HomepagePolicyManager();
        }
        return sInstance;
    }

    /**
     * If policies such as HomepageLocation are enabled on this device, the home page will be marked
     * as managed.
     *
     * @return True if the current home page is managed by enterprise policy.
     */
    public static boolean isHomepageLocationManaged() {
        return getInstance().isHomepageLocationPolicyManaged();
    }

    /**
     * @return The homepage URL from the homepage preference.
     */
    public static @NonNull GURL getHomepageUrl() {
        return getInstance().getHomepageLocationPolicyUrl();
    }

    /**
     * @return True if ShowHomeButton policy is managed/enabled by enterprise.
     */
    public static boolean isShowHomeButtonManaged() {
        return getInstance().isShowHomeButtonPolicyManaged();
    }

    /**
     * Returns the value of the ShowHomeButton policy, if it is enabled. Else throws an
     * AssertionError.
     */
    public static boolean getShowHomeButtonValue() {
        return getInstance().getShowHomeButtonPolicyValue();
    }

    /**
     * Returns whether the HomepagePolicyManager has been initialized with native. The
     * HomepagePolicyManager can only return valid result after initialing with native.
     */
    public static boolean isInitializedWithNative() {
        return getInstance().isInitialized();
    }

    /**
     * Adds a HomepagePolicyStateListener to receive updates when the homepage policy changes.
     * @param listener Object that would like to listen to changes from homepage policy.
     */
    public void addListener(HomepagePolicyStateListener listener) {
        mListeners.addObserver(listener);
    }

    /**
     * Stop observing pref changes and destroy the singleton instance.
     * Will be called from {@link org.chromium.chrome.browser.ChromeActivitySessionTracker}.
     */
    public static void destroy() {
        sInstance.destroyInternal();
        sInstance = null;
    }

    public static void setInstanceForTests(HomepagePolicyManager instance) {
        var oldValue = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    @VisibleForTesting
    HomepagePolicyManager() {
        mIsInitializedWithNative = false;
        mPrefChangeRegistrar = null;

        // Update feature flag related setting
        mSharedPreferenceManager = ChromeSharedPreferences.getInstance();

        String homepageLocationPolicyGurlSerialized =
                mSharedPreferenceManager.readString(
                        ChromePreferenceKeys.HOMEPAGE_LOCATION_POLICY_GURL, null);
        if (homepageLocationPolicyGurlSerialized != null) {
            mHomepageUrl = GURL.deserialize(homepageLocationPolicyGurlSerialized);
        } else {
            String homepageLocationPolicy;
            homepageLocationPolicy =
                    mSharedPreferenceManager.readString(
                            ChromePreferenceKeys.DEPRECATED_HOMEPAGE_LOCATION_POLICY, null);
            if (homepageLocationPolicy != null) {
                // This url comes from a native gurl that is written into PrefService as a string,
                // so we shouldn't need to call fixupUrl.
                mHomepageUrl = new GURL(homepageLocationPolicy);
            } else {
                mHomepageUrl = GURL.emptyGURL();
            }
        }

        mIsHomepageLocationManaged = !mHomepageUrl.isEmpty();

        if (ChromeFeatureList.sShowHomeButtonPolicyAndroid.isEnabled()) {
            mIsHomeButtonManaged =
                    mSharedPreferenceManager.readBoolean(
                            ChromePreferenceKeys.SHOW_HOME_BUTTON_POLICY_MANAGED, false);
            if (mIsHomeButtonManaged) {
                mHomeButtonPolicyValue =
                        mSharedPreferenceManager.readBoolean(
                                ChromePreferenceKeys.SHOW_HOME_BUTTON_POLICY_VALUE, true);
            }
        }

        ChromeBrowserInitializer.getInstance()
                .runNowOrAfterFullBrowserStarted(this::onFinishNativeInitialization);
    }

    /**
     * Constructor for unit tests.
     * @param prefChangeRegistrar Instance of {@link PrefChangeRegistrar} or test mocking.
     * @param listener Object extends {@link HomepagePolicyStateListener}. Will be added between
     *         singleton {@link HomepagePolicyManager} created, and have it initialized with {@link
     *         #initializeWithNative(PrefChangeRegistrar)} so that it will get the update from
     *         {@link HomepagePolicyStateListener#onHomepagePolicyUpdate()}.
     */
    @VisibleForTesting
    HomepagePolicyManager(
            @NonNull PrefChangeRegistrar prefChangeRegistrar,
            @Nullable HomepagePolicyStateListener listener) {
        this();

        if (listener != null) addListener(listener);
        initializeWithNative(prefChangeRegistrar);
    }

    /**
     * Initialize the instance with preference registrar, and start listen to changes for homepage
     * preference.
     * @param prefChangeRegistrar Instance of {@link PrefChangeRegistrar} or test mocking.
     */
    @VisibleForTesting
    void initializeWithNative(PrefChangeRegistrar prefChangeRegistrar) {
        mPrefChangeRegistrar = prefChangeRegistrar;
        mPrefChangeRegistrar.addObserver(Pref.HOME_PAGE, this);
        mPrefChangeRegistrar.addObserver(Pref.SHOW_HOME_BUTTON, this);

        mIsInitializedWithNative = true;
        refresh();
    }

    @Override
    public void onPreferenceChange() {
        refresh();
    }

    private void destroyInternal() {
        if (mPrefChangeRegistrar != null) mPrefChangeRegistrar.destroy();
        mListeners.clear();
    }

    private void refresh() {
        assert mIsInitializedWithNative;
        PrefService prefService = getPrefService();
        boolean isHomepageLocationManaged = prefService.isManagedPreference(Pref.HOME_PAGE);
        @NonNull GURL homepage = GURL.emptyGURL();
        if (isHomepageLocationManaged) {
            String homepagePref = prefService.getString(Pref.HOME_PAGE);
            assert homepagePref != null;
            // This url comes from a native gurl that is written into PrefService as a string,
            // so we shouldn't need to call fixupUrl.
            homepage = new GURL(homepagePref);
        }

        boolean isHomeButtonManaged = false;
        boolean homeButtonPolicyVal = mHomeButtonPolicyValue;
        if (ChromeFeatureList.sShowHomeButtonPolicyAndroid.isEnabled()) {
            isHomeButtonManaged = prefService.isManagedPreference(Pref.SHOW_HOME_BUTTON);
            if (isHomeButtonManaged) {
                homeButtonPolicyVal = prefService.getBoolean(Pref.SHOW_HOME_BUTTON);
            }
        }

        // Early return when nothing changes
        if (isHomepageLocationManaged == mIsHomepageLocationManaged
                && isHomeButtonManaged == mIsHomeButtonManaged
                && homeButtonPolicyVal == mHomeButtonPolicyValue
                && homepage.equals(mHomepageUrl)) {
            return;
        }

        mIsHomepageLocationManaged = isHomepageLocationManaged;
        mHomepageUrl = homepage;

        mIsHomeButtonManaged = isHomeButtonManaged;
        mHomeButtonPolicyValue = homeButtonPolicyVal;

        // Update shared preference
        mSharedPreferenceManager.writeString(
                ChromePreferenceKeys.HOMEPAGE_LOCATION_POLICY_GURL, mHomepageUrl.serialize());
        if (ChromeFeatureList.sShowHomeButtonPolicyAndroid.isEnabled()) {
            mSharedPreferenceManager.writeBoolean(
                    ChromePreferenceKeys.SHOW_HOME_BUTTON_POLICY_MANAGED, isHomeButtonManaged);
            mSharedPreferenceManager.writeBoolean(
                    ChromePreferenceKeys.SHOW_HOME_BUTTON_POLICY_VALUE, homeButtonPolicyVal);
        } else {
            mSharedPreferenceManager.removeKey(
                    ChromePreferenceKeys.SHOW_HOME_BUTTON_POLICY_MANAGED);
            mSharedPreferenceManager.removeKey(ChromePreferenceKeys.SHOW_HOME_BUTTON_POLICY_VALUE);
        }

        // Update the listeners about the status
        for (HomepagePolicyStateListener listener : mListeners) {
            listener.onHomepagePolicyUpdate();
        }
    }

    /** Called when the native library has finished loading. */
    private void onFinishNativeInitialization() {
        if (!mIsInitializedWithNative) {
            initializeWithNative(
                    PrefServiceUtil.createFor(ProfileManager.getLastUsedRegularProfile()));
        }
    }

    private PrefService getPrefService() {
        if (sPrefServiceForTesting != null) return sPrefServiceForTesting;
        return UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
    }

    public static void setPrefServiceForTesting(PrefService prefService) {
        sPrefServiceForTesting = prefService;
        ResettersForTesting.register(() -> sPrefServiceForTesting = null);
    }

    @VisibleForTesting
    public boolean isHomepageLocationPolicyManaged() {
        return mIsHomepageLocationManaged;
    }

    @VisibleForTesting
    public @NonNull GURL getHomepageLocationPolicyUrl() {
        assert mIsHomepageLocationManaged;
        return mHomepageUrl;
    }

    @VisibleForTesting
    public boolean isShowHomeButtonPolicyManaged() {
        return mIsHomeButtonManaged;
    }

    @VisibleForTesting
    public boolean getShowHomeButtonPolicyValue() {
        assert mIsHomeButtonManaged;
        return mHomeButtonPolicyValue;
    }

    @VisibleForTesting
    boolean isInitialized() {
        return mIsInitializedWithNative;
    }

    ObserverList<HomepagePolicyStateListener> getListenersForTesting() {
        return mListeners;
    }
}
