// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import org.chromium.base.annotations.NativeMethods;

/**
 * A Java API for recording Link to Text metrics.
 */
public class LinkToTextMetricsBridge {
    public static void logGenerateErrorTabHidden() {
        LinkToTextMetricsBridgeJni.get().logGenerateErrorTabHidden();
    }

    public static void logGenerateErrorOmniboxNavigation() {
        LinkToTextMetricsBridgeJni.get().logGenerateErrorOmniboxNavigation();
    }

    public static void logGenerateErrorTabCrash() {
        LinkToTextMetricsBridgeJni.get().logGenerateErrorTabCrash();
    }

    public static void logGenerateErrorIFrame() {
        LinkToTextMetricsBridgeJni.get().logGenerateErrorIFrame();
    }

    @NativeMethods
    interface Natives {
        void logGenerateErrorTabHidden();
        void logGenerateErrorOmniboxNavigation();
        void logGenerateErrorTabCrash();
        void logGenerateErrorIFrame();
    }
}
