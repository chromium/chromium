// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;

import org.chromium.base.IntentUtils;
import org.chromium.components.embedder_support.contextmenu.ContextMenuItemDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.Clipboard;
import org.chromium.url.GURL;

/** Handles the context menu item functionality in WebView. */
public class AwContextMenuItemDelegate implements ContextMenuItemDelegate {
    private final Activity mActivity;
    private final WebContents mWebContents;

    /** Builds a {@link AwContextMenuItemDelegate} instance. */
    public AwContextMenuItemDelegate(
            Activity activity, WebContents webContents, ContextMenuParams params) {
        mActivity = activity;
        mWebContents = webContents;
    }

    @Override
    public void onDestroy() {}

    @Override
    public String getPageTitle() {
        return mWebContents.getTitle();
    }

    @Override
    public WebContents getWebContents() {
        return mWebContents;
    }

    @Override
    public void onSaveToClipboard(String text, int clipboardType) {
        Clipboard.getInstance().setText(text);
    }

    @Override
    public void onOpenInDefaultBrowser(GURL url) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url.getSpec()));
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        IntentUtils.safeStartActivity(mActivity, intent);
    }

    @Override
    public GURL getPageUrl() {
        return mWebContents.getVisibleUrl();
    }
}
