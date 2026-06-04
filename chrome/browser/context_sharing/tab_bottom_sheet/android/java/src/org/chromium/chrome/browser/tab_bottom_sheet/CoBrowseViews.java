// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assertNonNull;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
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
    private final SettableNullableObservableSupplier<WebContents> mWebContentsSupplier =
            ObservableSuppliers.createNullable();

    private final @Nullable TabBottomSheetWebUi mWebUi;
    private final @Nullable ContextualTasksFusebox mFusebox;
    private final @ColorInt int mBackgroundColor;
    private final View mContainerView;
    private final @TabBottomSheetClientType int mClientType;
    private final @CoBrowseContainerType int mContainerType;
    private final @Nullable TabBottomSheetContentProvider mContentProvider;
    private @Nullable View mPeekView;

    /**
     * Constructor for CoBrowseViews.
     *
     * @param containerView The root view for the co-browse content.
     * @param clientType The client using the bottom sheet.
     * @param containerType The type of container hosting the views.
     * @param webUi The web UI for the view.
     * @param fusebox The fusebox for the view.
     * @param backgroundColor The background color for the view.
     * @param contentProvider The provider for custom sheet content implementations.
     */
    public CoBrowseViews(
            View containerView,
            @TabBottomSheetClientType int clientType,
            @CoBrowseContainerType int containerType,
            @Nullable TabBottomSheetWebUi webUi,
            @Nullable ContextualTasksFusebox fusebox,
            @ColorInt int backgroundColor,
            @Nullable TabBottomSheetContentProvider contentProvider) {
        mClientType = clientType;
        mContainerType = containerType;
        mWebUi = webUi;
        mFusebox = fusebox;
        mBackgroundColor = backgroundColor;
        mContainerView = containerView;
        mContentProvider = contentProvider;
        mWebContentsSupplier.set(getWebContents());
        populateViewHierarchy();
        updateForContainerType();
    }

    /** Returns the custom content provider if one was specified, null otherwise. */
    public @Nullable TabBottomSheetContentProvider getContentProvider() {
        return mContentProvider;
    }

    /** Destroys the co-browse view and its components. */
    @CalledByNative
    @VisibleForTesting
    void destroy() {
        ViewGroup webUiContainer = mContainerView.findViewById(R.id.web_ui_container);
        ViewGroup fuseboxContainer = mContainerView.findViewById(R.id.fusebox_container);
        ViewGroup peekContainer = mContainerView.findViewById(R.id.peek_view_container);
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
        ViewGroup peekContainer = mContainerView.findViewById(R.id.peek_view_container);
        peekContainer.removeAllViews();
        detachFromParent(peekView);
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
            ViewGroup peekContainer = mContainerView.findViewById(R.id.peek_view_container);
            peekContainer.removeView(mPeekView);
            mPeekView = null;
        }
    }

    /** Sets the WebContents of the WebUi. */
    @CalledByNative
    public void setWebContents(
            @Nullable @JniType("content::WebContents*") WebContents webContents,
            boolean requestFocus) {
        if (mWebUi != null) {
            View oldView = mWebUi.getWebUiView();
            mWebUi.setWebContents(webContents, requestFocus);
            mWebContentsSupplier.set(webContents);
            View newView = mWebUi.getWebUiView();
            if (oldView != newView) {
                ViewGroup webUiContainer = mContainerView.findViewById(R.id.web_ui_container);
                webUiContainer.removeAllViews();
                detachFromParent(newView);
                webUiContainer.addView(newView);
            }
        }
    }

    void setIgnoreClearFocus(boolean ignoreClearFocus) {
        if (mWebUi != null) {
            mWebUi.setIgnoreClearFocus(ignoreClearFocus);
        }
    }

    void setAllowFullscreenIme(boolean allow) {
        if (mWebUi != null) {
            mWebUi.setAllowFullscreenIme(allow);
        }
    }

    @TabBottomSheetClientType
    int getClientType() {
        return mClientType;
    }

    @Nullable WebContents getWebContents() {
        return mWebUi != null ? mWebUi.getWebContents() : null;
    }

    public NullableObservableSupplier<WebContents> getWebContentsSupplier() {
        return mWebContentsSupplier;
    }

    @Nullable WebViewResizingHelper getWebViewResizingHelper() {
        return mWebUi != null ? mWebUi.getWebViewResizingHelper() : null;
    }

    private void populateViewHierarchy() {
        ViewGroup webUiContainer = mContainerView.findViewById(R.id.web_ui_container);
        ViewGroup fuseboxContainer = mContainerView.findViewById(R.id.fusebox_container);
        ViewGroup peekContainer = mContainerView.findViewById(R.id.peek_view_container);

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

    private void updateForContainerType() {
        ViewGroup webUiContainer = mContainerView.findViewById(R.id.web_ui_container);

        if (mContainerType == CoBrowseContainerType.SIDE_PANEL) {
            View handleBar = mContainerView.findViewById(R.id.handle_bar);
            if (handleBar != null) {
                handleBar.setVisibility(View.GONE);
            }

            MarginLayoutParams layoutParams = (MarginLayoutParams) webUiContainer.getLayoutParams();
            layoutParams.topMargin = 0;
            webUiContainer.setLayoutParams(layoutParams);
        }
    }
}
