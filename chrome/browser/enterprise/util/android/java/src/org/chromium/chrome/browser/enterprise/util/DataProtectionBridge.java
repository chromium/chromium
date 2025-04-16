// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.util;

import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.RenderFrameHost;

/** Provides access to the enterprise data protection utility methods. */
@NullMarked
public class DataProtectionBridge {
    /**
     * Runs the provided callback after verifying that copying the specified text is allowed by the
     * current data protection policies. The callback boolean input will be true if the copy action
     * is allowed, or false if the copy action has been blocked or cancelled.
     *
     * @param text The text being copied.
     * @param renderFrameHost The RenderFrameHost providing the context in which a copy occurred.
     * @param callback The callback to run after verifying the policy.
     */
    public static void verifyCopyIsAllowedByPolicy(
            String text, RenderFrameHost renderFrameHost, Callback<Boolean> callback) {
        DataProtectionBridgeJni.get().verifyCopyIsAllowedByPolicy(text, renderFrameHost, callback);
    }

    @NativeMethods
    interface Natives {
        void verifyCopyIsAllowedByPolicy(
                String text, RenderFrameHost renderFrameHost, Callback<Boolean> callback);
    }
}
