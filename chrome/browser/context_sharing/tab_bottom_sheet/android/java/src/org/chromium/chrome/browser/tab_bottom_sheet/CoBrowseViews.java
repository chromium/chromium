// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assertNonNull;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFusebox;
import org.chromium.content_public.browser.WebContents;

/**
 * Class responsible for holding the co-browse view and its respective components. NOTE: Owner is
 * responsible for destroying this object.
 */
@NullMarked
public class CoBrowseViews {
    private final @Nullable TabBottomSheetWebUi mWebUi;
    private final @Nullable ContextualTasksFusebox mFusebox;
    private final @ColorInt int mBackgroundColor;
    private final View mContainerView;
    private final @TabBottomSheetClientType int mClientType;
    private @Nullable View mPeekView;

    /**
     * Constructor for CoBrowseViews.
     *
     * @param containerView The root view for the co-browse content.
     * @param clientType The client using the bottom sheet.
     * @param webUi The web UI for the view.
     * @param fusebox The fusebox for the view.
     * @param backgroundColor The background color for the view.
     */
    public CoBrowseViews(
            View containerView,
            @TabBottomSheetClientType int clientType,
            @Nullable TabBottomSheetWebUi webUi,
            @Nullable ContextualTasksFusebox fusebox,
            @ColorInt int backgroundColor) {
        mClientType = clientType;
        mWebUi = webUi;
        mFusebox = fusebox;
        mBackgroundColor = backgroundColor;
        mContainerView = containerView;
        populateViewHierarchy();
    }

    /** Destroys the co-browse view and its components. */
    @CalledByNative
    @VisibleForTesting
    void destroy() {
        ViewGroup webUiContainer = mContainerView.findViewById(R.id.web_ui_container);
        ViewGroup fuseboxContainer = mContainerView.findViewById(R.id.fusebox_container);
        ViewGroup peekContainer = mContainerView.findViewById(R.id.actor_control_container);
        if (mWebUi != null) {
            webUiContainer.removeAllViews();
            mWebUi.destroy();
        }
        if (mFusebox != null) {
            fuseboxContainer.removeAllViews();
            mFusebox.destroy();
        }
        if (mPeekView != null) {
            peekContainer.removeAllViews();
            mPeekView = null;
        }
    }

    /** Returns the background color for the co-browse view. */
    public @ColorInt int getBackgroundColor() {
        return mBackgroundColor;
    }

    /** Sets the touch handler for the Web UI container. */
    public void setWebUiTouchHandler(TabBottomSheetWebUiContainer.TouchHandler touchHandler) {
        TabBottomSheetWebUiContainer webUiContainer =
                assertNonNull(mContainerView.findViewById(R.id.web_ui_container));
        webUiContainer.setTouchHandler(touchHandler);
    }

    /** Returns the view for the co-browse content. */
    @CalledByNative
    public View getView() {
        return mContainerView;
    }

    public boolean hasPeekView() {
        return mPeekView != null;
    }

    /**
     * Attaches the peek view for the co-browse content.
     *
     * @param peekView The peek view to attach.
     */
    public void attachPeekView(View peekView) {
        ViewGroup peekContainer = mContainerView.findViewById(R.id.actor_control_container);
        detachFromParent(peekView);
        assert peekContainer.getChildCount() == 0;
        mPeekView = peekView;
        peekContainer.addView(mPeekView);
    }

    /**
     * Detaches the peek view if it matches the provided view.
     *
     * @param peekView The peek view to be removed.
     */
    public void removePeekView(View peekView) {
        if (mPeekView == peekView) {
            ViewGroup peekContainer = mContainerView.findViewById(R.id.actor_control_container);
            peekContainer.removeView(mPeekView);
            mPeekView = null;
        }
    }

    /** Sets the WebContents of the WebUi. */
    @CalledByNative
    public void setWebContents(
            @Nullable @JniType("content::WebContents*") WebContents webContents) {
        if (mWebUi != null) {
            View oldView = mWebUi.getWebUiView();
            mWebUi.setWebContents(webContents);
            View newView = mWebUi.getWebUiView();
            if (oldView != newView) {
                ViewGroup webUiContainer = mContainerView.findViewById(R.id.web_ui_container);
                webUiContainer.removeAllViews();
                detachFromParent(newView);
                webUiContainer.addView(newView);
            }
        }
        if (webContents != null) {
            webContents.getEventForwarder().setCurrentTouchOffsetX(0.0f);
            webContents.getEventForwarder().setCurrentTouchOffsetY(0.0f);
        }
    }

    @TabBottomSheetClientType
    int getClientType() {
        return mClientType;
    }

    @Nullable WebContents getWebContents() {
        return mWebUi != null ? mWebUi.getWebContents() : null;
    }

    @Nullable WebViewResizingHelper getWebViewResizingHelper() {
        return mWebUi != null ? mWebUi.getWebViewResizingHelper() : null;
    }

    private void populateViewHierarchy() {
        ViewGroup webUiContainer = mContainerView.findViewById(R.id.web_ui_container);
        ViewGroup fuseboxContainer = mContainerView.findViewById(R.id.fusebox_container);
        ViewGroup peekContainer = mContainerView.findViewById(R.id.actor_control_container);

        if (mWebUi != null) {
            View webUiView = mWebUi.getWebUiView();
            detachFromParent(webUiView);
            webUiContainer.addView(webUiView);
        }
        if (mFusebox != null) {
            View fuseboxView = mFusebox.getFuseboxView();
            detachFromParent(fuseboxView);
            fuseboxContainer.addView(fuseboxView);
        }
        if (mPeekView != null) {
            detachFromParent(mPeekView);
            peekContainer.addView(mPeekView);
        }
    }

    private void detachFromParent(View view) {
        if (view == null) return;

        final ViewGroup parent = (ViewGroup) view.getParent();
        if (parent == null) return;

        parent.removeView(view);
    }
}
