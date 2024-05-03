// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_image_service;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.page_image_service.mojom.ClientId;
import org.chromium.url.GURL;

/** Allows java access to the native ImageService. */
public class ImageServiceBridge {
    private long mNativeImageServiceBridge;

    /**
     * @param profile Used to acquire dependencies in native.
     */
    public ImageServiceBridge(Profile profile) {
        mNativeImageServiceBridge = ImageServiceBridgeJni.get().init(profile);
    }

    public void destroy() {
        ImageServiceBridgeJni.get().destroy(mNativeImageServiceBridge);
    }

    /**
     * Fetch the salient image {@link GURL} for the given page {@link GURL}.
     *
     * @param isAccountData Whether the underlying datatype being fetched for is account-bound.
     * @param clientId The ImageService client the salient image url is being fetched for.
     * @param pageUrl The url of the page the salient image is being fetched for.
     * @param callback The callback to receive the salient image url.
     */
    public void fetchImageUrlFor(
            boolean isAccountData,
            @ClientId.EnumType int clientId,
            GURL pageUrl,
            Callback<GURL> callback) {
        ImageServiceBridgeJni.get()
                .fetchImageUrlFor(
                        mNativeImageServiceBridge, isAccountData, clientId, pageUrl, callback);
    }

    @NativeMethods
    public interface Natives {
        long init(@JniType("Profile*") Profile profile);

        void destroy(long nativeImageServiceBridge);

        void fetchImageUrlFor(
                long nativeImageServiceBridge,
                boolean isAccountData,
                @ClientId.EnumType int clientId,
                @JniType("GURL") GURL pageUrl,
                Callback<GURL> callback);
    }
}
