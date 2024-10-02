// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;

import java.util.HashMap;
import java.util.Map;

/** Bridge, providing access to the native-side Privacy Sandbox configuration. */
public class PrivacySandboxSurveyBridge {

    private final Profile mProfile;

    public PrivacySandboxSurveyBridge(Profile profile) {
        mProfile = profile;
    }

    @CalledByNative
    private static Object createSentimentSurveyPsb() {
        return new HashMap<String, Boolean>();
    }

    @CalledByNative
    private static void insertSurveyBitIntoMap(Map<String, Boolean> map, String key, boolean bit) {
        if (map.containsKey(key)) {
            return;
        }
        map.put(key, bit);
    }

    public Map<String, Boolean> getPrivacySandboxSentimentSurveyPsb() {
        return PrivacySandboxSurveyBridgeJni.get().getPrivacySandboxSentimentSurveyPsb(mProfile);
    }

    @NativeMethods
    public interface Natives {
        Map<String, Boolean> getPrivacySandboxSentimentSurveyPsb(Profile profile);
    }
}
