// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce.shopping_list;

import androidx.annotation.Nullable;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkMeta;
import org.chromium.content_public.browser.WebContents;

/** A bridge for accessing the native ShoppingDataProvider from java. */
@JNINamespace("shopping_list::android")
public class ShoppingDataProviderBridge {
    private static final String TAG = "ShoppingDataProvider";

    /**
     * Gets the shopping data for the last navigation of the provided web contents if it exists.
     *
     * @param webContents The web contents to get the shopping data for.
     * @return An object containing the shopping data or null.
     */
    public static @Nullable PowerBookmarkMeta getForWebContents(WebContents webContents) {
        if (webContents == null) return null;

        byte[] protoBytes = ShoppingDataProviderBridgeJni.get().getForWebContents(webContents);
        if (protoBytes == null) return null;

        try {
            return PowerBookmarkMeta.parseFrom(protoBytes);
        } catch (InvalidProtocolBufferException ex) {
            Log.w(TAG, "Failed to parse shopping data: " + ex.getMessage());
            return null;
        }
    }

    @NativeMethods
    interface Natives {
        byte[] getForWebContents(WebContents webContents);
    }
}
