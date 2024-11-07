// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.WebContents;

/**
 * Helper for triggering the Safety Hub HaTS survey. Holds the state for the last requested survey
 * and triggers the HaTS survey when a WebContents becomes available.
 */
class SafetyHubHatsHelper extends EmptyTabObserver implements Destroyable {
    @VisibleForTesting static final String CONTROL_NOTIFICATION_MODULE = "none";
    private static ProfileKeyedMap<SafetyHubHatsHelper> sProfileMap;

    private final Profile mProfile;

    private TabModelSelector mCurrentTabModelSelector;
    private CurrentTabObserver mCurrentTabObserver;

    private String mModuleType;
    private boolean mHasTappedCard;

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
                        mProfile, webContents, mModuleType, mHasTappedCard);
        if (didShowSurvey) {
            removeObserver();
        }
    }

    @Override
    public void destroy() {
        removeObserver();
    }
}
