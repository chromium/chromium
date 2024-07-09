// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import android.view.View;

import org.jni_zero.CalledByNative;

import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;

/** A helper class that handles generating and dismissing context menus for {@link WebContents}. */
public class AwContextMenuHelper {

    private final WebContents mWebContents;
    private long mNativeAwContextMenuHelper;

    private AwContextMenuHelper(long nativeAwContextMenuHelper, WebContents webContents) {
        mNativeAwContextMenuHelper = nativeAwContextMenuHelper;
        mWebContents = webContents;
    }

    @CalledByNative
    private static AwContextMenuHelper create(
            long nativeAwContextMenuHelper, WebContents webContents) {
        return new AwContextMenuHelper(nativeAwContextMenuHelper, webContents);
    }

    @CalledByNative
    private void destroy() {
        dismissContextMenu();
        mNativeAwContextMenuHelper = 0;
    }

    // TODO(crbug.com/323344356) Create implementation of showContextMenu
    @CalledByNative
    private void showContextMenu(
            final ContextMenuParams params,
            RenderFrameHost renderFrameHost,
            View view,
            float topContentOffsetPx) {}

    // TODO(crbug.com/323344356) Create implementation of dismissContextMenu
    @CalledByNative
    private void dismissContextMenu() {}

    private void displayContextMenu() {}
}
