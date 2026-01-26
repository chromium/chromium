// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Abstract class for Tab Bottom Sheet toolbars. */
@NullMarked
public class TabBottomSheetWebUi {
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final ThinWebView mThinWebView;
    private @Nullable WebContents mWebContents;

    TabBottomSheetWebUi(Context context, WindowAndroid windowAndroid) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mThinWebView =
                ThinWebViewFactory.create(
                        mContext,
                        new ThinWebViewConstraints(),
                        assumeNonNull(mWindowAndroid.getIntentRequestTracker()));
    }

    void setWebContents(WebContents webContents) {
        mWebContents = webContents;
        ContentView contentView = ContentView.createContentView(mContext, mWebContents);
        assumeNonNull(mThinWebView).attachWebContents(mWebContents, contentView, null);
    }

    @Nullable WebContents getWebContents() {
        return mWebContents;
    }

    void destroy() {
        // We expect the life cycle of webContents to be managed by native.
        mWebContents = null;
        mThinWebView.destroy();
    }

    View getWebUiView() {
        return mThinWebView.getView();
    }
}
