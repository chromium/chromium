// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.rappor;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * JNI bridge to the native Rappor service from Java.
 */
@JNINamespace("rappor")
public final class RapporServiceBridge {
    private RapporServiceBridge() {
        // Only for static use.
    }

    public static void sampleString(String metric, String sampleValue) {
        RapporServiceBridgeJni.get().sampleString(metric, sampleValue);
    }

    public static void sampleDomainAndRegistryFromURL(String metric, String url) {
        RapporServiceBridgeJni.get().sampleDomainAndRegistryFromURL(metric, url);
    }

    @NativeMethods
    interface Natives {
        void sampleDomainAndRegistryFromURL(String metric, String url);
        void sampleString(String metric, String sampleValue);
    }
}
