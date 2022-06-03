// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.navigation_predictor;

import org.chromium.base.annotations.NativeMethods;

/**
 * Exposes methods to report tabs moving to foreground/background.
 */
public class NavigationPredictorBridge {
    private NavigationPredictorBridge() {}

    /**
     * Reports to native that tabbed activity has resumed.
     */
    public static void onActivityWarmResumed() {
        NavigationPredictorBridgeJni.get().onActivityWarmResumed();
    }

    /**
     * Reports to native that tabbed activity has started.
     */
    public static void onColdStart() {
        NavigationPredictorBridgeJni.get().onColdStart();
    }

    /**
     * Reports to native that tabbed activity has paused (moved to background).
     */
    public static void onPause() {
        NavigationPredictorBridgeJni.get().onPause();
    }

    @NativeMethods
    interface Natives {
        void onActivityWarmResumed();
        void onColdStart();
        void onPause();
    }
}
