// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.WebContents;

/** Java equivalent of safety_hub_hats_bridge.cc */
class SafetyHubHatsBridge implements Destroyable {
    @VisibleForTesting static final String CONTROL_NOTIFICATION_MODULE = "none";
    private static ProfileKeyedMap<SafetyHubHatsBridge> sProfileMap;

    private final Profile mProfile;

    private CurrentTabObserver mCurrentTabObserver;
    private boolean mDidShowSurvey;

    static SafetyHubHatsBridge getForProfile(Profile profile) {
        if (sProfileMap == null) {
            sProfileMap =
                    ProfileKeyedMap.createMapOfDestroyables(
                            ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);
        }
        return sProfileMap.getForProfile(profile, SafetyHubHatsBridge::new);
    }

    @VisibleForTesting
    SafetyHubHatsBridge(Profile profile) {
        mProfile = profile;
    }

    void triggerControlHatsSurvey(TabModelSelector tabModelSelector) {
        triggerHatsSurveyIfEnabled(tabModelSelector, CONTROL_NOTIFICATION_MODULE);
    }

    void triggerProactiveHatsSurvey(TabModelSelector tabModelSelector, String moduleType) {
        triggerHatsSurveyIfEnabled(tabModelSelector, moduleType);
    }

    private void triggerHatsSurveyIfEnabled(TabModelSelector tabModelSelector, String moduleType) {
        if (!ChromeFeatureList.sSafetyHubAndroidSurvey.isEnabled()) return;
        if (mCurrentTabObserver == null && !mDidShowSurvey) {
            mCurrentTabObserver =
                    new CurrentTabObserver(
                            tabModelSelector.getCurrentTabSupplier(),
                            new EmptyTabObserver() {
                                @Override
                                public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                                    if (tab == null) return;
                                    WebContents webContents = tab.getWebContents();
                                    if (!tab.isOffTheRecord() && webContents != null) {
                                        mDidShowSurvey =
                                                SafetyHubHatsBridgeJni.get()
                                                        .triggerHatsSurveyIfEnabled(
                                                                mProfile, webContents, moduleType);
                                        if (mDidShowSurvey) {
                                            removeObserver();
                                        }
                                    }
                                }
                            },
                            /* swapCallback= */ null);
        }
    }

    private void removeObserver() {
        if (mCurrentTabObserver != null) {
            mCurrentTabObserver.destroy();
            mCurrentTabObserver = null;
        }
    }

    @Override
    public void destroy() {
        removeObserver();
    }

    @NativeMethods
    interface Natives {
        boolean triggerHatsSurveyIfEnabled(
                @JniType("Profile*") Profile profile,
                WebContents webContents,
                @JniType("std::string") String moduleType);
    }
}
