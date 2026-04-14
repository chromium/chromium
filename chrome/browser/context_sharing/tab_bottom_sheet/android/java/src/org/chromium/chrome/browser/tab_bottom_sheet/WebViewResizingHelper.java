// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.thinwebview.ThinWebView;

/** Helper class for showing placeholders while resizing the Web View in the Tab Bottom Sheet. */
@NullMarked
public class WebViewResizingHelper {
    private final Context mContext;
    private final FrameLayout mResizingContainer;
    private @Nullable ThinWebView mThinWebView;

    public WebViewResizingHelper(Context context) {
        mContext = context;
        mResizingContainer = new FrameLayout(mContext);
    }

    public void reset() {
        if (mThinWebView == null) return;
        mResizingContainer.removeAllViews();
        mThinWebView = null;
    }

    public void setThinWebView(ThinWebView thinWebView) {
        reset();
        mThinWebView = thinWebView;

        mResizingContainer.addView(mThinWebView.getView());
    }

    public View getResizingContainer() {
        return mResizingContainer;
    }

    public void setIsResizing(boolean isResizing) {
        // TODO(crbug.com/486916366): Implement resizing logic.
    }
}
