// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assertNonNull;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.content_public.browser.WebContents;

import java.util.function.Supplier;

/**
 * Class responsible for holding the co-browse view and its respective components. NOTE: Owner is
 * responsible for destroying this object.
 */
@NullMarked
public class CoBrowseViews {
    private final SettableNullableObservableSupplier<WebContents> mWebContentsSupplier =
            ObservableSuppliers.createNullable();

    private final @Nullable TabBottomSheetWebUi mWebUi;
    private final @ColorInt int mBackgroundColor;
    private final @ColorInt int mPlaceholderElemColor;
    private final View mContainerView;
    private final @TabBottomSheetClientType int mClientType;
    private final @CoBrowseContainerType int mContainerType;
    private final @Nullable CoBrowseComponentProvider mContentProvider;
    private final Supplier<@Nullable PeekViewManager> mPeekViewManagerSupplier;
    private final Runnable mOnCloseClicked;
    private final Callback<@Nullable Boolean> mPlaceholderAllowedCallback =
            this::onPlaceholderAllowedChanged;
    private final Callback<@Nullable WebContents> mWebContentsObserver = this::onWebContentsChanged;

    private final TabBottomSheetWebUiContainer mWebUiContainer;
    private final ViewGroup mPeekContainer;
    private final ViewGroup mLoadingPlaceholderContainer;

    private @Nullable PeekViewManager mPeekViewManager;
    private @Nullable View mPeekView;
    private boolean mIsPlaceholderSetUp;
    private boolean mIsLoading;
    private @Nullable View mPlaceholderView;
    private @Nullable View mLoadingPlaceholderView;
    private @Nullable NullableObservableSupplier<Boolean> mPlaceholderAllowedSupplier;

    /**
     * Constructor for CoBrowseViews.
     *
     * @param containerView The root view for the co-browse content.
     * @param clientType The client using the bottom sheet.
     * @param containerType The type of container hosting the views.
     * @param webUi The web UI for the view.
     * @param backgroundColor The background color for the view.
     * @param placeholderElemColor The fill color for loading skeleton items.
     * @param contentProvider The provider for custom sheet content implementations.
     * @param peekViewManagerSupplier Supplier for the manager for the peek view.
     * @param onCloseClicked Run when the loading placeholder close button is clicked.
     */
    public CoBrowseViews(
            View containerView,
            @TabBottomSheetClientType int clientType,
            @CoBrowseContainerType int containerType,
            @Nullable TabBottomSheetWebUi webUi,
            @ColorInt int backgroundColor,
            @ColorInt int placeholderElemColor,
            @Nullable CoBrowseComponentProvider contentProvider,
            Supplier<@Nullable PeekViewManager> peekViewManagerSupplier,
            Runnable onCloseClicked) {
        mClientType = clientType;
        mContainerType = containerType;
        mWebUi = webUi;
        mBackgroundColor = backgroundColor;
        mPlaceholderElemColor = placeholderElemColor;
        mContainerView = containerView;
        mContentProvider = contentProvider;
        mPeekViewManagerSupplier = peekViewManagerSupplier;
        mOnCloseClicked = onCloseClicked;

        // Cache view lookups.
        mWebUiContainer = assertNonNull(containerView.findViewById(R.id.web_ui_container));
        mPeekContainer = assertNonNull(containerView.findViewById(R.id.peek_view_container));
        mLoadingPlaceholderContainer =
                assertNonNull(containerView.findViewById(R.id.loading_placeholder_container));

        mWebContentsSupplier.set(getWebContents());
        populateViewHierarchy();
        updateForContainerType();
        setupPlaceholder();
    }

    /** Returns the peek view manager if one was specified, null otherwise. */
    public @Nullable PeekViewManager getOrCreatePeekViewManager() {
        if (mPeekViewManager == null) {
            mPeekViewManager = mPeekViewManagerSupplier.get();
        }
        return mPeekViewManager;
    }

    /**
     * Destroys the cached {@link PeekViewManager}, if any, and clears the cache so that a fresh
     * instance is created the next time {@link #getOrCreatePeekViewManager()} is called.
     *
     * <p>The cached instance must never outlive its {@link PeekViewManager#destroy()} call: a
     * destroyed manager has unregistered its observers and will no longer update the peek view.
     */
    public void destroyPeekViewManager() {
        if (mPeekViewManager != null) {
            mPeekViewManager.destroy();
            mPeekViewManager = null;
        }
    }

    /** Returns the custom content provider if one was specified, null otherwise. */
    public @Nullable CoBrowseComponentProvider getContentProvider() {
        return mContentProvider;
    }

    /** Returns the background color for the co-browse view. */
    public @ColorInt int getBackgroundColor() {
        return mBackgroundColor;
    }

    /** Sets the touch handler for the Web UI container. */
    public void setWebUiTouchHandler(TabBottomSheetWebUiContainer.TouchHandler touchHandler) {
        mWebUiContainer.setTouchHandler(touchHandler);
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
        mPeekContainer.removeAllViews();
        detachFromParent(peekView);
        mPeekView = peekView;
        mPeekContainer.addView(mPeekView);
    }

    /**
     * Detaches the peek view if it matches the provided view.
     *
     * @param peekView The peek view to be removed.
     */
    public void removePeekView(View peekView) {
        if (mPeekView == peekView) {
            mPeekContainer.removeView(mPeekView);
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
                mWebUiContainer.removeAllViews();
                detachFromParent(newView);
                mWebUiContainer.addView(newView);
            }
        } else {
            mWebContentsSupplier.set(webContents);
        }
        updatePlaceholderVisibility();
    }

    /** Updates whether the native loading placeholder should be shown. */
    @CalledByNative
    public void setIsLoading(boolean isLoading) {
        if (mIsLoading == isLoading) {
            return;
        }
        mIsLoading = isLoading;
        updatePlaceholderVisibility();
    }

    /**
     * Sets the supplier that determines whether the placeholder is allowed to be shown.
     *
     * @param supplier The supplier that determines whether the placeholder is allowed to be shown.
     */
    public void setPlaceholderAllowedSupplier(NullableObservableSupplier<Boolean> supplier) {
        if (mPlaceholderAllowedSupplier != null) {
            mPlaceholderAllowedSupplier.removeObserver(mPlaceholderAllowedCallback);
        }
        mPlaceholderAllowedSupplier = supplier;
        if (mPlaceholderAllowedSupplier != null) {
            mPlaceholderAllowedSupplier.addSyncObserver(mPlaceholderAllowedCallback);
        }
        updatePlaceholderVisibility();
    }

    public NullableObservableSupplier<WebContents> getWebContentsSupplier() {
        return mWebContentsSupplier;
    }

    /** Destroys the co-browse view and its components. */
    @CalledByNative
    @VisibleForTesting
    void destroy() {
        mWebContentsSupplier.removeObserver(mWebContentsObserver);
        if (mPlaceholderAllowedSupplier != null) {
            mPlaceholderAllowedSupplier.removeObserver(mPlaceholderAllowedCallback);
        }
        if (mLoadingPlaceholderView != null) {
            mLoadingPlaceholderContainer.removeAllViews();
            mLoadingPlaceholderContainer.setVisibility(View.GONE);
            mLoadingPlaceholderView = null;
            mWebUiContainer.setVisibility(View.VISIBLE);
        }
        // Release the native callback (if JNI-backed) now that the loading placeholder, the only
        // holder of a click path to it, has been detached.
        if (mOnCloseClicked instanceof Destroyable destroyable) {
            destroyable.destroy();
        }
        if (mWebUi != null) {
            mWebUiContainer.removeAllViews();
            mWebUi.destroy();
        }
        if (mPeekView != null) {
            mPeekContainer.removeAllViews();
            mPeekView = null;
        }
        destroyPeekViewManager();
    }

    /** Returns whether the placeholder view is set up. */
    boolean isPlaceholderSetUp() {
        return mIsPlaceholderSetUp;
    }

    @Nullable View getLoadingPlaceholderViewForTesting() {
        return mLoadingPlaceholderView;
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

    @CalledByNative
    @Nullable WebContents getWebContents() {
        return mWebUi != null ? mWebUi.getWebContents() : null;
    }

    @Nullable WebViewResizingHelper getWebViewResizingHelper() {
        return mWebUi != null ? mWebUi.getWebViewResizingHelper() : null;
    }

    private void populateViewHierarchy() {
        if (mWebUi != null) {
            View webUiView = mWebUi.getWebUiView();
            detachFromParent(webUiView);
            mWebUiContainer.addView(webUiView);
        }
        if (mPeekView != null) {
            detachFromParent(mPeekView);
            mPeekContainer.addView(mPeekView);
        }
    }

    private void detachFromParent(View view) {
        if (view == null) return;

        final ViewGroup parent = (ViewGroup) view.getParent();
        if (parent == null) return;

        parent.removeView(view);
    }

    private void updateForContainerType() {
        if (mContainerType == CoBrowseContainerType.SIDE_PANEL) {
            removeTopMargin(mWebUiContainer);
            removeTopMargin(mLoadingPlaceholderContainer);
        }
    }

    private static void removeTopMargin(View view) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) view.getLayoutParams();
        layoutParams.topMargin = 0;
        view.setLayoutParams(layoutParams);
    }

    private void setupPlaceholder() {
        mPlaceholderView = mContainerView.findViewById(R.id.empty_placeholder_container);
        assert mPlaceholderView instanceof TextViewWithCompoundDrawables;

        if (mContentProvider != null) {
            mIsPlaceholderSetUp =
                    mContentProvider.setupPlaceholderView(
                            (TextViewWithCompoundDrawables) mPlaceholderView);
            mLoadingPlaceholderView =
                    mContentProvider.createLoadingPlaceholderView(
                            mContainerView.getContext(),
                            mBackgroundColor,
                            mPlaceholderElemColor,
                            mContainerType == CoBrowseContainerType.SIDE_PANEL,
                            mOnCloseClicked);
            if (mLoadingPlaceholderView != null) {
                mLoadingPlaceholderContainer.addView(
                        mLoadingPlaceholderView,
                        new FrameLayout.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT,
                                ViewGroup.LayoutParams.MATCH_PARENT));
            }
        }
        mWebContentsSupplier.addSyncObserverAndCallIfNonNull(mWebContentsObserver);
    }

    private void onPlaceholderAllowedChanged(@Nullable Boolean allowed) {
        updatePlaceholderVisibility();
    }

    private void onWebContentsChanged(@Nullable WebContents webContents) {
        updatePlaceholderVisibility();
    }

    private void updatePlaceholderVisibility() {
        boolean webContentsNull = mWebContentsSupplier.get() == null;
        boolean hasLoadingPlaceholder = mLoadingPlaceholderView != null;
        boolean showLoading = hasLoadingPlaceholder && mIsLoading;
        if (hasLoadingPlaceholder) {
            int loadingVisibility = showLoading ? View.VISIBLE : View.GONE;
            int webUiVisibility = showLoading ? View.INVISIBLE : View.VISIBLE;
            mLoadingPlaceholderContainer.setVisibility(loadingVisibility);
            mWebUiContainer.setVisibility(webUiVisibility);
        }

        if (mPlaceholderView == null) return;

        boolean isPlaceholderAllowed =
                mPlaceholderAllowedSupplier == null
                        || Boolean.TRUE.equals(mPlaceholderAllowedSupplier.get());
        boolean showPlaceholder =
                !showLoading && mIsPlaceholderSetUp && isPlaceholderAllowed && webContentsNull;
        mPlaceholderView.setVisibility(showPlaceholder ? View.VISIBLE : View.GONE);
    }
}
