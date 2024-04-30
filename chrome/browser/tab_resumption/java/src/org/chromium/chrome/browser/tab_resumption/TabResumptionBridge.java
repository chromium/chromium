// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;

/** The glue code between Tab resumption module and the native fetch and rank services backend. */
public class TabResumptionBridge {
    // The callback to be called when ranking the suggestions are completed.
    interface SuggestionRankedCallback {
        void onSuggestionRanked(SuggestionBundle bundle);
    }

    private long mNativeTabResumptionBridge;

    TabResumptionBridge(Profile profile) {
        mNativeTabResumptionBridge = TabResumptionBridgeJni.get().init(profile);
    }

    void rank(SuggestionBundle bundle, SuggestionRankedCallback callback) {
        // TODO(b/337858147): Rank the bundle in native.
        callback.onSuggestionRanked(bundle);
    }

    /**
     * Clean up the C++ side of this class. After the call, this class instance shouldn't be used.
     */
    void destroy() {
        TabResumptionBridgeJni.get().destroy(mNativeTabResumptionBridge);
        mNativeTabResumptionBridge = 0;
    }

    @NativeMethods
    interface Natives {
        long init(@JniType("Profile*") Profile profile);

        void destroy(long nativeTabResumptionBridge);
    }
}
