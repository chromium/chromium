// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/** Java equivalent of safety_hub_hats_bridge.cc */
class SafetyHubHatsBridge {
    /** Tries to trigger the HaTS survey if the flag is enabled. */
    static boolean triggerHatsSurveyIfEnabled(
            Profile profile,
            WebContents webContents,
            String moduleType,
            boolean hasTappedCard,
            boolean hasVisited,
            String globalState) {
        if (!ChromeFeatureList.sSafetyHubAndroidSurvey.isEnabled()) {
            return false;
        }

        return SafetyHubHatsBridgeJni.get()
                .triggerHatsSurveyIfEnabled(
                        profile, webContents, moduleType, hasTappedCard, hasVisited, globalState);
    }

    @NativeMethods
    interface Natives {
        boolean triggerHatsSurveyIfEnabled(
                @JniType("Profile*") Profile profile,
                WebContents webContents,
                @JniType("std::string") String moduleType,
                boolean hasTappedCard,
                boolean hasVisited,
                @JniType("std::string") String globalState);
    }
}
