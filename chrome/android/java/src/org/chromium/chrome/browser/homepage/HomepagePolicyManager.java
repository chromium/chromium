// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceUtil;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils.BooleanPolicyState;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrar.PrefObserver;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.url.GURL;

/**
 * Provides information for the home page related policies. Monitors changes for the homepage
 * preference.
 */
@NullMarked
public class HomepagePolicyManager implements PrefObserver {
    /** An interface to receive updates from {@link HomepagePolicyManager}. */
    public interface HomepagePolicyStateListener {
        /**
         * Will be called when homepage policy status change. Though cases are rare, when homepage
         * policy has changed during runtime, listeners will receive updates.
         */
        void onHomepagePolicyUpdate();
    }

    private static @Nullable HomepagePolicyManager sInstance;

    private static @Nullable PrefService sPrefServiceForTesting;
    private static @Nullable GURL sHomepageUrlForTesting;
    private static @Nullable Boolean sHomepageIsNtpForTesting;
    private static @Nullable Boolean sIsHomepageManagedForTesting;
    private static @Nullable Boolean sIsInitializedWithNativeForTesting;

    private boolean mIsHomepageLocationManaged;
    private GURL mHomepageUrl;

    @BooleanPolicyState private int mHomeButtonPolicyState;
    @BooleanPolicyState private int mHomepageSelectionPolicyState;

    private boolean mIsInitializedWithNative;
    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;

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

    public static void setHomepageForTesting(boolean isManaged, GURL homepageUrl, boolean isNtp) {
        sIsHomepageManagedForTesting = isManaged;
        sHomepageUrlForTesting = homepageUrl;
        sHomepageIsNtpForTesting = isNtp;
        ResettersForTesting.register(
                () -> {
                    sIsHomepageManagedForTesting = null;
                    sHomepageUrlForTesting = null;
                    sHomepageIsNtpForTesting = null;
                });
    }

    /**
     * If policies such as HomepageLocation are enabled on this device, the home page will be marked
     * as managed.
     *
     * @return True if the current home page is managed by enterprise policy.
     */
    public static boolean isHomepageLocationManaged() {
        if (sIsHomepageManagedForTesting != null) {
            return sIsHomepageManagedForTesting;
        }
        return getInstance().isHomepageLocationPolicyManaged();
    }

    /**
     * @return The homepage URL from the homepage preference.
     */
    public static GURL getHomepageUrl() {
        if (sHomepageUrlForTesting != null) {
            return sHomepageUrlForTesting;
        }
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
     * @return True if ShowHomeButton has a recommended value from an enterprise policy.
     */
    public static boolean isShowHomeButtonRecommended() {
        return getInstance().isShowHomeButtonPolicyRecommended();
    }

    /**
     * @return True if the user's setting for ShowHomeButton matches the recommended value.
     */
    public static boolean isFollowingHomepageButtonRecommendation() {
        return getInstance().isFollowingHomepageButtonPolicyRecommendation();
    }

    /**
     * @return True if the homepage location or homepageIsNTP have recommended value(s) from an
     *     enterprise policy.
     */
    public static boolean isHomepageSelectionRecommended() {
        return getInstance().isHomepageSelectionPolicyRecommended();
    }

    /**
     * @return True if the user's selection for the homepage matches the recommended value of all
     *     recommendations set by the homepage location and homeIsNTP enterprise policies.
     */
    public static boolean isFollowingHomepageSelectionRecommendation() {
        return getInstance().isFollowingHomepageSelectionPolicyRecommendation();
    }

    /**
     * Sets the user's preference for the homepage location. This method is the bridge to the native
     * PrefService.
     *
     * @param homepageUrl The user's desired homepage URL.
     */
    public static void setNativeHomepageLocation(String homepageUrl) {
        getInstance().getPrefService().setString(Pref.HOME_PAGE, homepageUrl);
    }

    /**
     * Sets the user's preference for whether the home button is shown in the native. This method is
     * the bridge to the native PrefService. Prefer calling {@link
     * HomepageManager#setPrefHomepageEnabled()} to keep native and java in sync.
     *
     * @param enabled The user's desired setting for showing the home button.
     */
    public static void setNativeShowHomeButtonState(boolean enabled) {
        getInstance().getPrefService().setBoolean(Pref.SHOW_HOME_BUTTON, enabled);
    }

    /**
     * Sets the user's preference for whether the homepage is the new tab page. This method is the
     * bridge to the native PrefService.
     *
     * @param isNtp The user's desired setting for whether the homepage is the new tab page.
     */
    public static void setNativeHomepageIsNtp(boolean isNtp) {
        getInstance().getPrefService().setBoolean(Pref.HOME_PAGE_IS_NEW_TAB_PAGE, isNtp);
    }

    /**
     * @return true if HomepageIsNewTabPage policy is managed/enabled by enterprise.
     */
    public static boolean isHomepageNewTabPageManaged() {
        return getInstance().isHomepageIsNtpPolicyManaged();
    }

    /**
     * Returns the value of the HomepageIsNewTabPage policy, if it is enabled. Else throws an
     * AssertionError.
     */
    public static boolean getHomepageNewTabPageValue() {
        return getInstance().getHomepageIsNtpPolicyValue();
    }

    /**
     * Returns true if HomepageIsNewTabPage policy is managed and has a value of true, else false.
     */
    public static boolean isHomepageNewTabPageEnabled() {
        if (sHomepageIsNtpForTesting != null) {
            return sHomepageIsNtpForTesting;
        }
        return isHomepageNewTabPageManaged() && getHomepageNewTabPageValue();
    }

    /**
     * Returns whether the HomepagePolicyManager has been initialized with native. The
     * HomepagePolicyManager can only return valid result after initialing with native.
     */
    public static boolean isInitializedWithNative() {
        if (sIsInitializedWithNativeForTesting != null) {
            return sIsInitializedWithNativeForTesting;
        }
        return getInstance().isInitialized();
    }

    public static void setIsInitializedWithNativeForTesting(boolean isInitialized) {
        sIsInitializedWithNativeForTesting = isInitialized;
        ResettersForTesting.register(() -> sIsInitializedWithNativeForTesting = null);
    }

    /**
     * Adds a HomepagePolicyStateListener to receive updates when the homepage policy changes.
     *
     * @param listener Object that would like to listen to changes from homepage policy.
     */
    public void addListener(HomepagePolicyStateListener listener) {
        mListeners.addObserver(listener);
    }

    /**
     * Stop observing pref changes and destroy the singleton instance. Will be called from {@link
     * org.chromium.chrome.browser.ChromeActivitySessionTracker}.
     */
    @SuppressWarnings("NullAway")
    public static void destroy() {
        if (sInstance == null) return;
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
            mHomeButtonPolicyState =
                    mSharedPreferenceManager.readInt(
                            ChromePreferenceKeys.SHOW_HOME_BUTTON_POLICY_STATE,
                            BooleanPolicyState.UNMANAGED);
        }

        if (ChromeFeatureList.sHomepageIsNewTabPagePolicyAndroid.isEnabled()) {
            mHomepageSelectionPolicyState =
                    mSharedPreferenceManager.readInt(
                            ChromePreferenceKeys.HOMEPAGE_SELECTION_POLICY_STATE,
                            BooleanPolicyState.UNMANAGED);
        }

        ChromeBrowserInitializer.getInstance()
                .runNowOrAfterFullBrowserStarted(this::onFinishNativeInitialization);
    }

    /**
     * Constructor for unit tests.
     *
     * @param prefChangeRegistrar Instance of {@link PrefChangeRegistrar} or test mocking.
     * @param listener Object extends {@link HomepagePolicyStateListener}. Will be added between
     *     singleton {@link HomepagePolicyManager} created, and have it initialized with {@link
     *     #initializeWithNative(PrefChangeRegistrar)} so that it will get the update from {@link
     *     HomepagePolicyStateListener#onHomepagePolicyUpdate()}.
     */
    @VisibleForTesting
    HomepagePolicyManager(
            PrefChangeRegistrar prefChangeRegistrar,
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
        mPrefChangeRegistrar.addObserver(Pref.HOME_PAGE_IS_NEW_TAB_PAGE, this);

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
        GURL homepage = GURL.emptyGURL();
        if (isHomepageLocationManaged) {
            String homepagePref = prefService.getString(Pref.HOME_PAGE);
            assert homepagePref != null;
            // This url comes from a native gurl that is written into PrefService as a string,
            // so we shouldn't need to call fixupUrl.
            homepage = new GURL(homepagePref);
        }

        @BooleanPolicyState int homeButtonPolicyState = BooleanPolicyState.UNMANAGED;
        if (ChromeFeatureList.sShowHomeButtonPolicyAndroid.isEnabled()) {
            boolean isManaged = prefService.isManagedPreference(Pref.SHOW_HOME_BUTTON);
            if (isManaged) {
                homeButtonPolicyState =
                        prefService.getBoolean(Pref.SHOW_HOME_BUTTON)
                                ? BooleanPolicyState.MANAGED_BY_POLICY_ON
                                : BooleanPolicyState.MANAGED_BY_POLICY_OFF;
            } else if (prefService.isFollowingRecommendation(Pref.SHOW_HOME_BUTTON)) {
                homeButtonPolicyState = BooleanPolicyState.RECOMMENDED_IS_FOLLOWED;
            } else if (prefService.hasRecommendation(Pref.SHOW_HOME_BUTTON)) {
                homeButtonPolicyState = BooleanPolicyState.RECOMMENDED_IS_NOT_FOLLOWED;
            }
        }

        @BooleanPolicyState int homepageSelectionPolicyState = BooleanPolicyState.UNMANAGED;
        if (ChromeFeatureList.sHomepageIsNewTabPagePolicyAndroid.isEnabled()) {
            boolean isHomepageNtpManaged =
                    prefService.isManagedPreference(Pref.HOME_PAGE_IS_NEW_TAB_PAGE);

            if (isHomepageNtpManaged) {
                homepageSelectionPolicyState =
                        prefService.getBoolean(Pref.HOME_PAGE_IS_NEW_TAB_PAGE)
                                ? BooleanPolicyState.MANAGED_BY_POLICY_ON
                                : BooleanPolicyState.MANAGED_BY_POLICY_OFF;
            } else if (isHomepageLocationManaged) {
                // Admin provided NTP is indistinguishable from HomepageIsNTP managed and true.
                boolean isNtp = UrlUtilities.isNtpUrl(homepage);
                homepageSelectionPolicyState =
                        isNtp
                                ? BooleanPolicyState.MANAGED_BY_POLICY_ON
                                : BooleanPolicyState.MANAGED_BY_POLICY_OFF;
            } else {
                boolean hasNtpRecommendation =
                        prefService.hasRecommendation(Pref.HOME_PAGE_IS_NEW_TAB_PAGE);
                boolean hasLocationRecommendation = prefService.hasRecommendation(Pref.HOME_PAGE);

                if (hasNtpRecommendation || hasLocationRecommendation) {
                    boolean isEitherRecommendationOverridden =
                            (hasNtpRecommendation
                                            && !prefService.isFollowingRecommendation(
                                                    Pref.HOME_PAGE_IS_NEW_TAB_PAGE))
                                    || (hasLocationRecommendation
                                            && !prefService.isFollowingRecommendation(
                                                    Pref.HOME_PAGE));
                    homepageSelectionPolicyState =
                            isEitherRecommendationOverridden
                                    ? BooleanPolicyState.RECOMMENDED_IS_NOT_FOLLOWED
                                    : BooleanPolicyState.RECOMMENDED_IS_FOLLOWED;

                    // If admin changes recommendation that user has not overridden, update prefs.
                    boolean usesLocationRecommendation =
                            prefService.isRecommendedPreference(Pref.HOME_PAGE);
                    boolean usesNtpRecommendation =
                            prefService.isRecommendedPreference(Pref.HOME_PAGE_IS_NEW_TAB_PAGE);
                    if (usesNtpRecommendation) {
                        boolean homepageIsNtp =
                                prefService.getBoolean(Pref.HOME_PAGE_IS_NEW_TAB_PAGE);
                        mSharedPreferenceManager.writeBoolean(
                                ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, homepageIsNtp);
                    }
                    if (usesLocationRecommendation) {
                        GURL homepageGURL = new GURL(prefService.getString(Pref.HOME_PAGE));
                        mSharedPreferenceManager.writeBoolean(
                                ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, false);
                        mSharedPreferenceManager.writeString(
                                ChromePreferenceKeys.HOMEPAGE_CUSTOM_GURL,
                                homepageGURL.serialize());
                    }
                }
            }
        }

        // Early return when nothing changes
        if (isHomepageLocationManaged == mIsHomepageLocationManaged
                && homeButtonPolicyState == mHomeButtonPolicyState
                && homepageSelectionPolicyState == mHomepageSelectionPolicyState
                && homepage.equals(mHomepageUrl)) {
            return;
        }

        mIsHomepageLocationManaged = isHomepageLocationManaged;
        mHomepageUrl = homepage;

        mHomeButtonPolicyState = homeButtonPolicyState;
        mHomepageSelectionPolicyState = homepageSelectionPolicyState;

        // Update shared preference
        mSharedPreferenceManager.writeString(
                ChromePreferenceKeys.HOMEPAGE_LOCATION_POLICY_GURL, mHomepageUrl.serialize());
        if (ChromeFeatureList.sShowHomeButtonPolicyAndroid.isEnabled()) {
            mSharedPreferenceManager.writeInt(
                    ChromePreferenceKeys.SHOW_HOME_BUTTON_POLICY_STATE, mHomeButtonPolicyState);
            // If admin changes recommendation that user has not overridden.
            if (prefService.isRecommendedPreference(Pref.SHOW_HOME_BUTTON)) {
                boolean enabled = prefService.getBoolean(Pref.SHOW_HOME_BUTTON);
                mSharedPreferenceManager.writeBoolean(
                        ChromePreferenceKeys.HOMEPAGE_ENABLED, enabled);
            }
        } else {
            mSharedPreferenceManager.removeKey(ChromePreferenceKeys.SHOW_HOME_BUTTON_POLICY_STATE);
        }
        if (ChromeFeatureList.sHomepageIsNewTabPagePolicyAndroid.isEnabled()) {
            mSharedPreferenceManager.writeInt(
                    ChromePreferenceKeys.HOMEPAGE_SELECTION_POLICY_STATE,
                    mHomepageSelectionPolicyState);
        } else {
            mSharedPreferenceManager.removeKey(
                    ChromePreferenceKeys.HOMEPAGE_SELECTION_POLICY_STATE);
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
    public GURL getHomepageLocationPolicyUrl() {
        assert mIsHomepageLocationManaged;
        return mHomepageUrl;
    }

    @VisibleForTesting
    public boolean isShowHomeButtonPolicyManaged() {
        return mHomeButtonPolicyState == BooleanPolicyState.MANAGED_BY_POLICY_ON
                || mHomeButtonPolicyState == BooleanPolicyState.MANAGED_BY_POLICY_OFF;
    }

    @VisibleForTesting
    public boolean getShowHomeButtonPolicyValue() {
        assert isShowHomeButtonPolicyManaged();
        return mHomeButtonPolicyState == BooleanPolicyState.MANAGED_BY_POLICY_ON;
    }

    @VisibleForTesting
    public boolean isShowHomeButtonPolicyRecommended() {
        return mHomeButtonPolicyState == BooleanPolicyState.RECOMMENDED_IS_FOLLOWED
                || mHomeButtonPolicyState == BooleanPolicyState.RECOMMENDED_IS_NOT_FOLLOWED;
    }

    @VisibleForTesting
    public boolean isFollowingHomepageButtonPolicyRecommendation() {
        assert isShowHomeButtonPolicyRecommended();
        return mHomeButtonPolicyState == BooleanPolicyState.RECOMMENDED_IS_FOLLOWED;
    }

    @VisibleForTesting
    public boolean isHomepageSelectionPolicyRecommended() {
        return mHomepageSelectionPolicyState == BooleanPolicyState.RECOMMENDED_IS_FOLLOWED
                || mHomepageSelectionPolicyState == BooleanPolicyState.RECOMMENDED_IS_NOT_FOLLOWED;
    }

    @VisibleForTesting
    public boolean isFollowingHomepageSelectionPolicyRecommendation() {
        assert isHomepageSelectionPolicyRecommended();
        return mHomepageSelectionPolicyState == BooleanPolicyState.RECOMMENDED_IS_FOLLOWED;
    }

    @VisibleForTesting
    public boolean isHomepageIsNtpPolicyManaged() {
        return mHomepageSelectionPolicyState == BooleanPolicyState.MANAGED_BY_POLICY_ON
                || mHomepageSelectionPolicyState == BooleanPolicyState.MANAGED_BY_POLICY_OFF;
    }

    @VisibleForTesting
    public boolean getHomepageIsNtpPolicyValue() {
        assert isHomepageIsNtpPolicyManaged();
        return mHomepageSelectionPolicyState == BooleanPolicyState.MANAGED_BY_POLICY_ON;
    }

    @VisibleForTesting
    boolean isInitialized() {
        return mIsInitializedWithNative;
    }

    ObserverList<HomepagePolicyStateListener> getListenersForTesting() {
        return mListeners;
    }
}
