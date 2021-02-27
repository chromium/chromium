// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.url.GURL;

/**
 * A Java API for connecting to shared_highlighting component.
 */
public class LinkToTextBridge {
    public static void logGenerateErrorTabHidden() {
        LinkToTextBridgeJni.get().logGenerateErrorTabHidden();
    }

    public static void logGenerateErrorOmniboxNavigation() {
        LinkToTextBridgeJni.get().logGenerateErrorOmniboxNavigation();
    }

    public static void logGenerateErrorTabCrash() {
        LinkToTextBridgeJni.get().logGenerateErrorTabCrash();
    }

    public static void logGenerateErrorIFrame() {
        LinkToTextBridgeJni.get().logGenerateErrorIFrame();
    }

    public static void logGenerateErrorBlockList() {
        LinkToTextBridgeJni.get().logGenerateErrorBlockList();
    }

    public static void logGenerateErrorTimeout() {
        LinkToTextBridgeJni.get().logGenerateErrorTimeout();
    }

    // TODO(gayane): Update the name whenever |shared_highlighting::ShouldOfferLinkToText| updated
    // to moredescriptive name.
    public static boolean shouldOfferLinkToText(GURL url) {
        return LinkToTextBridgeJni.get().shouldOfferLinkToText(url);
    }

    @NativeMethods
    interface Natives {
        void logGenerateErrorTabHidden();
        void logGenerateErrorOmniboxNavigation();
        void logGenerateErrorTabCrash();
        void logGenerateErrorIFrame();
        void logGenerateErrorBlockList();
        void logGenerateErrorTimeout();
        boolean shouldOfferLinkToText(GURL url);
    }
}
