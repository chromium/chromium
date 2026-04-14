// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;


import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
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

    /**
     * @param context The context for the view.
     */
    public WebViewResizingHelper(Context context) {
        mContext = context;

        mResizingContainer = new FrameLayout(mContext);
    }

    /** Resets the helper to its initial state. */
    public void reset() {
        if (mThinWebView == null) return;
        mResizingContainer.removeAllViews();
        mThinWebView = null;
    }

    /** Sets the ThinWebView which will be resized. */
    public void setThinWebView(ThinWebView thinWebView) {
        reset();
        mThinWebView = thinWebView;

        FrameLayout.LayoutParams layoutParams =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        layoutParams.gravity = Gravity.BOTTOM;
        mResizingContainer.addView(mThinWebView.getView(), layoutParams);
    }

    /** Returns the resizing container. This holds the ThinWebView and the placeholder. */
    public View getResizingContainer() {
        return mResizingContainer;
    }

    /**
     * Sets whether the web view is resizing. If true, the placeholder will be shown. If false, the
     * placeholder will be hidden.
     */
    public void setIsResizing(boolean isResizing) {
        if (mThinWebView == null) return;

        if (isResizing) {
            enableResizingMode();
        } else {
            disableResizingMode();
        }
    }

    private void enableResizingMode() {
        assert mThinWebView != null;

        View webView = mThinWebView.getView();
        ViewGroup.LayoutParams params = webView.getLayoutParams();
        if (params != null) {
            params.height = webView.getHeight();
            webView.setLayoutParams(params);
        }
    }

    private void disableResizingMode() {
        assert mThinWebView != null;

        View webView = mThinWebView.getView();
        ViewGroup.LayoutParams params = webView.getLayoutParams();
        if (params != null) {
            params.height = ViewGroup.LayoutParams.MATCH_PARENT;
            webView.setLayoutParams(params);
        }
    }
}
