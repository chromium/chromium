// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.provider.Settings;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ApplicationStateListener;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.metrics.VariationsSession;
import org.chromium.chrome.browser.notifications.NotificationPlatformBridge;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.password_manager.PasswordManagerLifecycleHelper;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.components.browser_ui.accessibility.DeviceAccessibilitySettingsHandler;
import org.chromium.components.browser_ui.accessibility.FontSizePrefs;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.HashMap;
import java.util.Map;

/**
 * Tracks the foreground session state for the Chrome activities.
 */
public class ChromeActivitySessionTracker {

    @SuppressLint("StaticFieldLeak")
    private static ChromeActivitySessionTracker sInstance;

    private final OmahaServiceStartDelayer mOmahaServiceStartDelayer =
            new OmahaServiceStartDelayer();
    private final Map<Activity, Supplier<TabModelSelector>> mTabModelSelectorSuppliers =
            new HashMap<>();

    // Used to trigger variation changes (such as seed fetches) upon application foregrounding.
    private final VariationsSession mVariationsSession;

    private boolean mIsInitialized;
    private boolean mIsStarted;

    /**
     * @return The activity session tracker for Chrome.
     */
    public static ChromeActivitySessionTracker getInstance() {
        ThreadUtils.assertOnUiThread();

        if (sInstance == null) sInstance = new ChromeActivitySessionTracker();
        return sInstance;
    }

    /**
     * Constructor exposed for extensibility only.
     * @see #getInstance()
     */
    protected ChromeActivitySessionTracker() {
        mVariationsSession = AppHooks.get().createVariationsSession();
    }

    /**
     * Register a supplier which returns the total tab count for the given application.
     * @param activity The activity associated with the given tab count supplier.
     * @param tabCountSupplier Supplies the current tab count for the given activity.
     */
    public void registerTabModelSelectorSupplier(
            Activity activity, Supplier<TabModelSelector> tabCountSupplier) {
        mTabModelSelectorSuppliers.put(activity, tabCountSupplier);
    }

    /**
     * Unregisters the supplier associated with the given activity.
     * @param activity Tha activity to unregister.
     */
    public void unregisterTabModelSelectorSupplier(Activity activity) {
        mTabModelSelectorSuppliers.remove(activity);
    }

    /**
     * Asynchronously returns the value of the "restrict" URL param that the variations service
     * should use for variation seed requests.
     * @param callback Callback that will be called with the param value when available.
     */
    public void getVariationsRestrictModeValue(Callback<String> callback) {
        mVariationsSession.getRestrictModeValue(callback);
    }

    /**
     * @return The latest country according to the current variations state. Null if not available.
     */
    public @Nullable String getVariationsLatestCountry() {
        return mVariationsSession.getLatestCountry();
    }

    /**
     * Handle any initialization that occurs once native has been loaded.
     */
    public void initializeWithNative() {
        ThreadUtils.assertOnUiThread();

        if (mIsInitialized) return;
        mIsInitialized = true;
        assert !mIsStarted;

        mVariationsSession.initializeWithNative();
        ApplicationStatus.registerApplicationStateListener(createApplicationStateListener());
    }

    /**
     * Each top-level activity (those extending {@link ChromeActivity}) should call this during
     * its onStart phase. When called for the first time, this marks the beginning of a foreground
     * session and calls onForegroundSessionStart(). Subsequent calls are noops until
     * onForegroundSessionEnd() is called, to handle changing top-level Chrome activities in one
     * foreground session.
     */
    public void onStartWithNative() {
        ThreadUtils.assertOnUiThread();

        if (mIsStarted) return;
        mIsStarted = true;

        assert mIsInitialized;

        onForegroundSessionStart();
    }

    /**
     * Called when a top-level Chrome activity (ChromeTabbedActivity, CustomTabActivity) is
     * started in foreground. It will not be called again when other Chrome activities take over
     * (see onStart()), that is, when correct activity calls startActivity() for another Chrome
     * activity.
     */
    private void onForegroundSessionStart() {
        try (TraceEvent te = TraceEvent.scoped(
                     "ChromeActivitySessionTracker.onForegroundSessionStart")) {
            UmaUtils.recordForegroundStartTimeWithNative();
            updatePasswordEchoState();
            FontSizePrefs.getInstance(Profile.getLastUsedRegularProfile())
                    .onSystemFontScaleChanged();
            DeviceAccessibilitySettingsHandler.getInstance(Profile.getLastUsedRegularProfile())
                    .updateFontWeightAdjustment();
            ChromeLocalizationUtils.recordUiLanguageStatus();
            updateAcceptLanguages();
            mVariationsSession.start();
            mOmahaServiceStartDelayer.onForegroundSessionStart();
            AppHooks.get().getChimeDelegate().startSession();
            PasswordManagerLifecycleHelper.getInstance().onStartForegroundSession();

            // Track the ratio of Chrome startups that are caused by notification clicks.
            // TODO(johnme): Add other reasons (and switch to recordEnumeratedHistogram).
            RecordHistogram.recordBooleanHistogram("Startup.BringToForegroundReason",
                    NotificationPlatformBridge.wasNotificationRecentlyClicked());
        }
    }

    /**
     * Called when last of Chrome activities is stopped, ending the foreground session. This will
     * not be called when a Chrome activity is stopped because another Chrome activity takes over.
     * This is ensured by ActivityStatus, which switches to track new activity when its started and
     * will not report the old one being stopped (see createStateListener() below).
     */
    private void onForegroundSessionEnd() {
        if (!mIsStarted) return;
        UmaUtils.recordBackgroundTimeWithNative();
        ProfileManagerUtils.flushPersistentDataForAllProfiles();
        mIsStarted = false;
        mOmahaServiceStartDelayer.onForegroundSessionEnd();

        IntentHandler.clearPendingReferrer();
        IntentHandler.clearPendingIncognitoUrl();

        int totalTabCount = 0;
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            Supplier<TabModelSelector> tabModelSelectorSupplier =
                    mTabModelSelectorSuppliers.get(activity);
            if (tabModelSelectorSupplier == null || !tabModelSelectorSupplier.hasValue()) continue;
            totalTabCount += tabModelSelectorSupplier.get().getTotalTabCount();
        }
        RecordHistogram.recordCount1MHistogram("Tab.TotalTabCount.BeforeLeavingApp", totalTabCount);

        Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
        tracker.notifyEvent(EventConstants.FOREGROUND_SESSION_DESTROYED);
    }

    private void onForegroundActivityDestroyed() {
        if (ApplicationStatus.isEveryActivityDestroyed()) {
            // These will all be re-initialized when a new Activity starts / upon next use.
            PartnerBrowserCustomizations.destroy();
            ShareImageFileUtils.clearSharedImages();
        }
    }

    private ApplicationStateListener createApplicationStateListener() {
        return newState -> {
            if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES) {
                onForegroundSessionEnd();
            } else if (newState == ApplicationState.HAS_DESTROYED_ACTIVITIES) {
                onForegroundActivityDestroyed();
            }
        };
    }

    /**
     * Update the accept languages after changing Android locale setting. Doing so kills the
     * Activities but it doesn't kill the Application, so this should be called in
     * {@link #onStart} instead of {@link #initialize}.
     */
    private void updateAcceptLanguages() {
        String currentLocale = LocaleUtils.getDefaultLocaleListString();
        String previousLocale = SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.APP_LOCALE, null);
        ChromeLocalizationUtils.recordLocaleUpdateStatus(previousLocale, currentLocale);
        if (!TextUtils.equals(previousLocale, currentLocale)) {
            SharedPreferencesManager.getInstance().writeString(
                    ChromePreferenceKeys.APP_LOCALE, currentLocale);
            TranslateBridge.resetAcceptLanguages(currentLocale);
            if (previousLocale != null) {
                // Clear cache so that accept-languages change can be applied immediately.
                // TODO(changwan): The underlying BrowsingDataRemover::Remove() is an asynchronous
                // call. So cache-clearing may not be effective if URL rendering can happen before
                // OnBrowsingDataRemoverDone() is called, in which case we may have to reload as
                // well. Check if it can happen.
                BrowsingDataBridge.getInstance().clearBrowsingData(
                        null, new int[] {BrowsingDataType.CACHE}, TimePeriod.ALL_TIME);
            }
        }
    }

    /**
     * Honor the Android system setting about showing the last character of a password for a short
     * period of time.
     */
    private void updatePasswordEchoState() {
        boolean systemEnabled =
                Settings.System.getInt(ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.System.TEXT_SHOW_PASSWORD, 1)
                == 1;
        if (UserPrefs.get(Profile.getLastUsedRegularProfile())
                        .getBoolean(Pref.WEB_KIT_PASSWORD_ECHO_ENABLED)
                == systemEnabled) {
            return;
        }

        UserPrefs.get(Profile.getLastUsedRegularProfile())
                .setBoolean(Pref.WEB_KIT_PASSWORD_ECHO_ENABLED, systemEnabled);
    }

    /**
     * @return The {@link OmahaServiceStartDelayer} for the browser process.
     */
    public OmahaServiceStartDelayer getOmahaServiceStartDelayerForTesting() {
        return mOmahaServiceStartDelayer;
    }
}
