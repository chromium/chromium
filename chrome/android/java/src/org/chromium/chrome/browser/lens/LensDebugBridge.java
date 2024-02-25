// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import org.jni_zero.CalledByNative;

/**
 * Bridge class to allow for Java <-> Native communication for Lens integrations. Currently used to
 * support communication required to populate the Lens Internals page.
 */
public class LensDebugBridge {
    /** Start collecting proactive debug data for future requests. */
    @CalledByNative
    public static void startProactiveDebugMode() {
        LensController.getInstance().enableDebugMode();
    }

    /** Stop collecting proactive debug data for future requests. */
    @CalledByNative
    public static void stopProactiveDebugMode() {
        LensController.getInstance().disableDebugMode();
    }

    /** Fetch all collected debug data for past requests for when the mode was enabled. */
    @CalledByNative
    public static String[][] refreshDebugData() {
        return LensController.getInstance().getDebugData();
    }
}
