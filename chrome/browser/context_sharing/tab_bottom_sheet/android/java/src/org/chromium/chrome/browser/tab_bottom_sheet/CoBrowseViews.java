// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.context_sharing.R;

/** Class responsible for holding the co-browse view and its respective components. */
@NullMarked
public class CoBrowseViews {
    private final @Nullable TabBottomSheetToolbar mToolbar;
    private final @Nullable TabBottomSheetWebUi mWebUi;
    private final @Nullable TabBottomSheetFusebox mFusebox;
    private final View mView;

    /**
     * Constructor for CoBrowseViews.
     *
     * @param context The context for the view.
     * @param toolbar The toolbar for the view.
     * @param webUi The web UI for the view.
     * @param fusebox The fusebox for the view.
     */
    public CoBrowseViews(
            Context context,
            @Nullable TabBottomSheetToolbar toolbar,
            @Nullable TabBottomSheetWebUi webUi,
            @Nullable TabBottomSheetFusebox fusebox) {
        mToolbar = toolbar;
        mWebUi = webUi;
        mFusebox = fusebox;
        mView = buildView(context);
    }

    /** Returns the view for the co-browse content. */
    public View getView() {
        return mView;
    }

    public void destroy() {
        ViewGroup toolbarContainer = mView.findViewById(R.id.toolbar_container);
        ViewGroup webUiContainer = mView.findViewById(R.id.web_ui_container);
        ViewGroup fuseboxContainer = mView.findViewById(R.id.fusebox_container);
        if (mToolbar != null) {
            toolbarContainer.removeAllViews();
        }
        if (mWebUi != null) {
            webUiContainer.removeAllViews();
            mWebUi.destroy();
        }
        if (mFusebox != null) {
            fuseboxContainer.removeAllViews();
            mFusebox.destroy();
        }
    }

    /** Sets the WebUI container's height. */
    public void setWebUiContainerHeight(int height) {
        ViewGroup webUiContainer = mView.findViewById(R.id.web_ui_container);
        LinearLayout.LayoutParams webUiContainerParams =
                (LinearLayout.LayoutParams) webUiContainer.getLayoutParams();

        if (webUiContainerParams.height != height) {
            webUiContainerParams.height = height;
            webUiContainerParams.weight = 0f;
            webUiContainer.setLayoutParams(webUiContainerParams);
        }
    }

    /** Sets the ThinWebView's height. */
    public void setThinWebViewHeight(int height) {
        if (mWebUi != null) {
            View content = mWebUi.getWebUiView();
            ViewGroup.LayoutParams contentParams = content.getLayoutParams();
            if (contentParams.height == ViewGroup.LayoutParams.MATCH_PARENT) {
                contentParams.height = height;
                content.setLayoutParams(contentParams);
            }
        }
    }

    /** Sets the ThinWebView's insets. */
    void setThinWebViewInsets(int top, int left, int bottom, int right) {
        if (mWebUi != null) {
            mWebUi.setInsets(top, left, bottom, right);
        }
    }

    int getThinWebViewHeight() {
        if (mWebUi != null) {
            return mWebUi.getWebUiView().getHeight();
        }
        return 0;
    }

    int getFuseboxHeight() {
        if (mFusebox != null) {
            return mFusebox.getFuseboxView().getHeight();
        }
        return 0;
    }

    int getToolbarHeight() {
        if (mToolbar != null) {
            return mToolbar.getToolbarView().getHeight();
        }
        return 0;
    }

    private View buildView(Context context) {
        View view = LayoutInflater.from(context).inflate(R.layout.tab_bottom_sheet, null);
        ViewGroup toolbarContainer = view.findViewById(R.id.toolbar_container);
        ViewGroup webUiContainer = view.findViewById(R.id.web_ui_container);
        ViewGroup fuseboxContainer = view.findViewById(R.id.fusebox_container);

        if (mToolbar != null) {
            toolbarContainer.addView(mToolbar.getToolbarView());
        }
        if (mWebUi != null) {
            webUiContainer.addView(mWebUi.getWebUiView());
        }
        if (mFusebox != null) {
            fuseboxContainer.addView(mFusebox.getFuseboxView());
        }

        return view;
    }
}
