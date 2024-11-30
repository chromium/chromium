// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.app.Activity;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleProperties.ModuleState;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.hats.SurveyClient;
import org.chromium.chrome.browser.ui.hats.SurveyClientFactory;
import org.chromium.chrome.browser.ui.hats.SurveyConfig;
import org.chromium.chrome.browser.ui.hats.SurveyUiDelegate;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.WebContents;

import java.util.Map;

/**
 * Helper for triggering the Safety Hub HaTS survey. Holds the state for the last requested survey
 * and triggers the HaTS survey when a WebContents becomes available.
 */
class SafetyHubHatsHelper extends EmptyTabObserver implements Destroyable {
    private static final String TAG = "SafetyHubHatsHelper";
    @VisibleForTesting static final String CONTROL_NOTIFICATION_MODULE = "none";
    private static final String SENTIMENT_ORGANIC_SURVEY_TRIGGER =
            "safety_hub_android_organic_survey";
    private static ProfileKeyedMap<SafetyHubHatsHelper> sProfileMap;

    private final Profile mProfile;

    private TabModelSelector mCurrentTabModelSelector;
    private CurrentTabObserver mCurrentTabObserver;

    private String mModuleType;
    private boolean mHasTappedCard;
    private boolean mHasVisited;

    private SafetyHubSurveyUiDelegate mSafetyHubSurveyUiDelegate = new SafetyHubSurveyUiDelegate();

    private static class SafetyHubSurveyUiDelegate implements SurveyUiDelegate {
        @Override
        public void showSurveyInvitation(
                Runnable onSurveyAccepted,
                Runnable onSurveyDeclined,
                Runnable onSurveyPresentationFailed) {
            // No invitation UI is shown, trigger the survey right away.
            assert onSurveyAccepted != null;
            onSurveyAccepted.run();
        }

        @Override
        public void dismiss() {
            // no-op.
        }
    }

    static SafetyHubHatsHelper getForProfile(Profile profile) {
        if (sProfileMap == null) {
            sProfileMap =
                    ProfileKeyedMap.createMapOfDestroyables(
                            ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);
        }
        return sProfileMap.getForProfile(profile, SafetyHubHatsHelper::new);
    }

    @VisibleForTesting
    SafetyHubHatsHelper(Profile profile) {
        mProfile = profile;
    }

    // Triggers the organic HaTS survey for Safety Hub. No invitation UI is shown when the survey is
    // triggered by this method.
    void triggerOrganicHatsSurvey(@NonNull Activity activity) {
        mHasVisited = true;
        if (!ChromeFeatureList.sSafetyHubAndroidOrganicSurvey.isEnabled()) {
            return;
        }

        SurveyConfig config = SurveyConfig.get(SENTIMENT_ORGANIC_SURVEY_TRIGGER);
        SurveyClient surveyClient =
                SurveyClientFactory.getInstance()
                        .createClient(config, mSafetyHubSurveyUiDelegate, mProfile);
        if (surveyClient == null) {
            Log.d(TAG, "SurveyClient is null. config: " + SurveyConfig.toString(config));
            return;
        }
        surveyClient.showSurvey(
                activity,
                /* lifecyclerDispatcher= */ null,
                getSurveyPsbBitValues(),
                getSurveyPsbStringValues());
    }

    void triggerControlHatsSurvey(TabModelSelector tabModelSelector) {
        mModuleType = CONTROL_NOTIFICATION_MODULE;
        mHasTappedCard = false;
        triggerHatsSurveyIfEnabled(tabModelSelector);
    }

    void triggerProactiveHatsSurveyWhenCardShown(
            TabModelSelector tabModelSelector, String moduleType) {
        mModuleType = moduleType;
        mHasTappedCard = false;
        triggerHatsSurveyIfEnabled(tabModelSelector);
    }

    void triggerProactiveHatsSurveyWhenCardTapped(
            TabModelSelector tabModelSelector, String moduleType) {
        if (!ChromeFeatureList.sSafetyHubAndroidSurveyV2.isEnabled()) {
            return;
        }

        mModuleType = moduleType;
        mHasTappedCard = true;
        triggerHatsSurveyIfEnabled(tabModelSelector);
    }

    /**
     * Tries to trigger the HaTS survey if the flag is enabled. It will always trigger the HaTS
     * survey with the latest information on the last requested `TabModelSelector`.
     */
    private void triggerHatsSurveyIfEnabled(TabModelSelector tabModelSelector) {
        if (!ChromeFeatureList.sSafetyHubAndroidSurvey.isEnabled()) {
            return;
        }

        if (!shouldUpdateCurrentTabObserver(tabModelSelector)) {
            return;
        }

        removeObserver();
        mCurrentTabModelSelector = tabModelSelector;
        mCurrentTabObserver =
                new CurrentTabObserver(mCurrentTabModelSelector.getCurrentTabSupplier(), this);
    }

    /**
     * Returns if the current tab observer needs to be updated.
     *
     * <p>The current tab observer needs to be updated if: (1) there is isn't one; or, (2) if the
     * current one is for a different tab model. We will update it so it tracks the tab model from
     * the latest HaTS request.
     */
    private boolean shouldUpdateCurrentTabObserver(TabModelSelector tabModelSelector) {
        return mCurrentTabObserver == null || mCurrentTabModelSelector != tabModelSelector;
    }

    private void removeObserver() {
        if (mCurrentTabObserver != null) {
            mCurrentTabObserver.destroy();
            mCurrentTabObserver = null;
            mCurrentTabModelSelector = null;
        }
    }

    @Override
    public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
        if (tab == null) {
            return;
        }
        WebContents webContents = tab.getWebContents();
        if (tab.isOffTheRecord() || webContents == null) {
            return;
        }
        boolean didShowSurvey =
                SafetyHubHatsBridge.triggerHatsSurveyIfEnabled(
                        mProfile,
                        webContents,
                        getModuleType(),
                        mHasTappedCard,
                        mHasVisited,
                        getOverallState());
        if (didShowSurvey) {
            removeObserver();
        }
    }

    @Override
    public void destroy() {
        removeObserver();
    }

    private Map<String, Boolean> getSurveyPsbBitValues() {
        return Map.of("Tapped card", mHasTappedCard, "Has visited", mHasVisited);
    }

    private Map<String, String> getSurveyPsbStringValues() {
        return Map.of(
                "Notification module type", getModuleType(), "Global state", getOverallState());
    }

    private @NonNull String getModuleType() {
        return mModuleType != null ? mModuleType : "";
    }

    /**
     * Returns a string that represents the overall state of Safety Hub. The overall state
     * represents the most severe state of all the modules. The logic for the state of each module
     * should be equivalent to {@link SafetyHubModuleViewBinder#getModuleState()}.
     */
    @VisibleForTesting
    @NonNull
    String getOverallState() {
        @ModuleState int[] moduleStates = new int[5];

        moduleStates[0] = getPasswordModuleState();
        moduleStates[1] = getUpdateCheckModuleState();
        moduleStates[2] = getPermissionsModuleState();
        moduleStates[3] = getNotificationModuleState();
        moduleStates[4] = getSafeBrowsingModuleState();

        @ModuleState int mostSevereState = ModuleState.SAFE;
        for (@ModuleState int state : moduleStates) {
            if (state < mostSevereState) {
                mostSevereState = state;
            }
        }

        switch (mostSevereState) {
            case ModuleState.WARNING:
                return "Warning";
            case ModuleState.UNAVAILABLE:
                return "Unavailable";
            case ModuleState.INFO:
                return "Info";
            case ModuleState.SAFE:
                return "Safe";
            default:
                throw new IllegalArgumentException();
        }
    }

    /**
     * Returns the state of the password module. This computation differs slightly from the one in
     * {@link SafetyHubModuleViewBinder#getModuleState()}.
     */
    private @ModuleState int getPasswordModuleState() {
        int compromisedPasswordsCount =
                UserPrefs.get(mProfile).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);
        int weakPasswordsCount = UserPrefs.get(mProfile).getInteger(Pref.WEAK_CREDENTIALS_COUNT);
        int reusedPasswordsCount =
                UserPrefs.get(mProfile).getInteger(Pref.REUSED_CREDENTIALS_COUNT);

        if (compromisedPasswordsCount > 0) {
            return ModuleState.WARNING;
        } else if (compromisedPasswordsCount < 0) {
            return ModuleState.UNAVAILABLE;
        } else if (weakPasswordsCount > 0 || reusedPasswordsCount > 0) {
            return ModuleState.INFO;
        }

        return ModuleState.SAFE;
    }

    private @ModuleState int getUpdateCheckModuleState() {
        return SafetyHubUtils.getUpdateCheckModuleState(
                SafetyHubFetchServiceFactory.getForProfile(mProfile).getUpdateStatus());
    }

    private @ModuleState int getPermissionsModuleState() {
        return SafetyHubUtils.getPermissionsModuleState(
                UnusedSitePermissionsBridge.getForProfile(mProfile).getRevokedPermissions().length);
    }

    private @ModuleState int getNotificationModuleState() {
        return SafetyHubUtils.getNotificationModuleState(
                NotificationPermissionReviewBridge.getForProfile(mProfile)
                        .getNotificationPermissions()
                        .size());
    }

    private @ModuleState int getSafeBrowsingModuleState() {
        return SafetyHubUtils.getSafeBrowsingModuleState(
                new SafeBrowsingBridge(mProfile).getSafeBrowsingState());
    }
}
