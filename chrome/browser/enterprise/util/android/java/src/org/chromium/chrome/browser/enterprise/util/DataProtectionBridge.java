// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.util;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.DATA_CONTROLS_SEARCH_WITH;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;

/** Provides access to the enterprise data protection utility methods. */
@NullMarked
public class DataProtectionBridge {
    private static DataProtectionBridge.@Nullable Natives sNativesForTesting;

    /**
     * Runs the provided callback after verifying that copying the specified text is allowed by the
     * current data protection policies. The callback boolean input will be true if the copy action
     * is allowed, or false if the copy action has been blocked or cancelled.
     *
     * <p>If enterprise policies are not enabled on the device, this check should cause minimal
     * delays.
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
        getJni().verifyCopyTextIsAllowedByPolicy(text, renderFrameHost, callback);
    }

    /**
     * Runs the provided callback after verifying that sharing the specified content is allowed by
     * the current data protection policies. The callback boolean input will be true if the share
     * action is allowed, or false if the share action has been blocked or cancelled.
     *
     * <p>If enterprise policies are not enabled on the device, this check should cause minimal
     * delays.
     *
     * @param text The text being shared.
     * @param renderFrameHost The RenderFrameHost providing the context in which a share occurred.
     * @param callback The callback to run after verifying the policy.
     */
    public static void verifyShareTextIsAllowedByPolicy(
            String text, RenderFrameHost renderFrameHost, Callback<Boolean> callback) {
        if (!ChromeFeatureList.isEnabled(ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)) {
            callback.onResult(true);
            return;
        }
        getJni().verifyShareTextIsAllowedByPolicy(text, renderFrameHost, callback);
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
        getJni().verifyCopyUrlIsAllowedByPolicy(url, renderFrameHost, callback);
    }

    /**
     * Runs the provided callback after verifying that sharing the specified content is allowed by
     * the current data protection policies. The callback boolean input will be true if the share
     * action is allowed, or false if the share action has been blocked or cancelled.
     *
     * <p>If enterprise policies are not enabled on the device, this check should cause minimal
     * delays.
     *
     * @param url The url being shared.
     * @param renderFrameHost The RenderFrameHost providing the context in which a share occurred.
     * @param callback The callback to run after verifying the policy.
     */
    public static void verifyShareUrlIsAllowedByPolicy(
            String url, RenderFrameHost renderFrameHost, Callback<Boolean> callback) {
        if (!ChromeFeatureList.isEnabled(ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)) {
            callback.onResult(true);
            return;
        }
        getJni().verifyShareUrlIsAllowedByPolicy(url, renderFrameHost, callback);
    }

    /**
     * Runs the provided callback after verifying that copying the specified image is allowed by the
     * current data protection policies. The callback boolean input will be true if the copy action
     * is allowed, or false if the copy action has been blocked or cancelled.
     *
     * @param imageUri The uri for the image being copied.
     * @param renderFrameHost The RenderFrameHost providing the context in which a copy occurred.
     * @param callback The callback to run after verifying the policy. The boolean input will be
     *     true if the copy action was allowed, false if the action was blocked or cancelled.
     */
    public static void verifyCopyImageIsAllowedByPolicy(
            String imageUri, RenderFrameHost renderFrameHost, Callback<Boolean> callback) {
        if (!ChromeFeatureList.isEnabled(ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)) {
            callback.onResult(true);
            return;
        }
        getJni().verifyCopyImageIsAllowedByPolicy(imageUri, renderFrameHost, callback);
    }

    /**
     * Runs the provided callback after verifying that sharing the specified content is allowed by
     * the current data protection policies. The callback boolean input will be true if the share
     * action is allowed, or false if the share action has been blocked or cancelled.
     *
     * <p>If enterprise policies are not enabled on the device, this check should cause minimal
     * delays.
     *
     * @param imageUri The uri for the image being copied.
     * @param renderFrameHost The RenderFrameHost providing the context in which a share occurred.
     * @param callback The callback to run after verifying the policy.
     */
    public static void verifyShareImageIsAllowedByPolicy(
            String imageUri, RenderFrameHost renderFrameHost, Callback<Boolean> callback) {
        if (!ChromeFeatureList.isEnabled(ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)) {
            callback.onResult(true);
            return;
        }
        getJni().verifyShareImageIsAllowedByPolicy(imageUri, renderFrameHost, callback);
    }

    /**
     * Runs the provided callback after verifying that the generic action for the specified image
     * content is allowed by the current clipboard copy data protection policies. The callback
     * boolean input will be true if the action is allowed, or false if the action has been blocked
     * or cancelled.
     *
     * <p>If enterprise policies are not enabled on the device, this check should cause minimal
     * delays.
     *
     * @param imageUri The uri for the image subject to the current action.
     * @param renderFrameHost The RenderFrameHost providing the context in which the action
     *     occurred.
     * @param callback The callback to run after verifying the policy.
     */
    public static void verifyGenericCopyImageActionIsAllowedByPolicy(
            String imageUri, RenderFrameHost renderFrameHost, Callback<Boolean> callback) {
        if (!ChromeFeatureList.isEnabled(ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID)) {
            callback.onResult(true);
            return;
        }
        getJni().verifyGenericCopyImageActionIsAllowedByPolicy(imageUri, renderFrameHost, callback);
    }

    /**
     * Checks if the user is allowed to use the "Search for..." context menu item in the given
     * WebContents based on DataControlsRules policies. Returns true if search is allowed, false
     * otherwise.
     */
    public static boolean isSearchWithAllowed(@Nullable WebContents webContents) {
        if (!ChromeFeatureList.isEnabled(DATA_CONTROLS_SEARCH_WITH)) {
            return true;
        }
        return getJni().isSearchWithAllowed(webContents);
    }

    /**
     * Checks if the user is allowed to use the "Search for..." context menu item in the given
     * WebContents based on DataControlsRules policies. If the action is allowed (or reported/warned
     * and bypassed), `callback` will be run.
     */
    public static void shouldAllowSearchWith(
            int textLength, @Nullable WebContents webContents, Runnable callback) {
        if (!ChromeFeatureList.isEnabled(DATA_CONTROLS_SEARCH_WITH)) {
            callback.run();
            return;
        }
        getJni().shouldAllowSearchWith(textLength, webContents, callback);
    }

    public static void setInstanceForTesting(DataProtectionBridge.Natives instance) {
        sNativesForTesting = instance;
    }

    private static DataProtectionBridge.Natives getJni() {
        if (sNativesForTesting != null) {
            return sNativesForTesting;
        }
        return DataProtectionBridgeJni.get();
    }

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        void verifyCopyTextIsAllowedByPolicy(
                String text,
                RenderFrameHost renderFrameHost,
                @JniType("base::OnceCallback<void(bool)>") Callback<Boolean> callback);

        void verifyCopyUrlIsAllowedByPolicy(
                String url,
                RenderFrameHost renderFrameHost,
                @JniType("base::OnceCallback<void(bool)>") Callback<Boolean> callback);

        void verifyCopyImageIsAllowedByPolicy(
                String imageUri,
                RenderFrameHost renderFrameHost,
                @JniType("base::OnceCallback<void(bool)>") Callback<Boolean> callback);

        void verifyShareTextIsAllowedByPolicy(
                String text,
                RenderFrameHost renderFrameHost,
                @JniType("base::OnceCallback<void(bool)>") Callback<Boolean> callback);

        void verifyShareUrlIsAllowedByPolicy(
                String url,
                RenderFrameHost renderFrameHost,
                @JniType("base::OnceCallback<void(bool)>") Callback<Boolean> callback);

        void verifyShareImageIsAllowedByPolicy(
                String imageUri,
                RenderFrameHost renderFrameHost,
                @JniType("base::OnceCallback<void(bool)>") Callback<Boolean> callback);

        void verifyGenericCopyImageActionIsAllowedByPolicy(
                String imageUri,
                RenderFrameHost renderFrameHost,
                @JniType("base::OnceCallback<void(bool)>") Callback<Boolean> callback);

        boolean isSearchWithAllowed(@Nullable WebContents webContents);

        void shouldAllowSearchWith(
                int textLength,
                @Nullable WebContents webContents,
                @JniType("base::OnceClosure") Runnable callback);
    }
}
