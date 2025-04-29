// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.util;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
     * @param callback The callback to run after verifying the policy. The boolean input will be
     *     true if the copy action was allowed, false if the action was blocked or cancelled.
     */
    public static void verifyCopyTextIsAllowedByPolicy(
            String text, RenderFrameHost renderFrameHost, Callback<Boolean> callback) {
        if (!ChromeFeatureList.isEnabled(ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)) {
            callback.onResult(true);
            return;
        }
        DataProtectionBridgeJni.get()
                .verifyCopyTextIsAllowedByPolicy(text, renderFrameHost, callback);
    }

    /**
     * Runs the provided callback after verifying that copying the specified url is allowed by the
     * current data protection policies. The callback boolean input will be true if the copy action
     * is allowed, or false if the copy action has been blocked or cancelled.
     *
     * @param url The url being copied.
     * @param renderFrameHost The RenderFrameHost providing the context in which a copy occurred.
     * @param callback The callback to run after verifying the policy. The boolean input will be
     *     true if the copy action was allowed, false if the action was blocked or cancelled.
     */
    public static void verifyCopyUrlIsAllowedByPolicy(
            String url, RenderFrameHost renderFrameHost, Callback<Boolean> callback) {
        if (!ChromeFeatureList.isEnabled(ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)) {
            callback.onResult(true);
            return;
        }
        DataProtectionBridgeJni.get()
                .verifyCopyUrlIsAllowedByPolicy(url, renderFrameHost, callback);
    }

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        void verifyCopyTextIsAllowedByPolicy(
                String text, RenderFrameHost renderFrameHost, Callback<Boolean> callback);

        void verifyCopyUrlIsAllowedByPolicy(
                String url, RenderFrameHost renderFrameHost, Callback<Boolean> callback);

        void verifyCopyImageIsAllowedByPolicy(
                String imageUri, RenderFrameHost renderFrameHost, Callback<Boolean> callback);
    }
}
