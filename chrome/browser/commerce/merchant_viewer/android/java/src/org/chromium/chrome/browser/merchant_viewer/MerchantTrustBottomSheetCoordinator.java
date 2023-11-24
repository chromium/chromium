// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

/** Coordinator for managing the merchant trust bottom sheet experience. */
public class MerchantTrustBottomSheetCoordinator implements View.OnLayoutChangeListener {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final View mLayoutView;
    private final MerchantTrustMetrics mMetrics;

    private MerchantTrustBottomSheetMediator mMediator;
    private BottomSheetObserver mBottomSheetObserver;
    private MerchantTrustBottomSheetContent mSheetContent;
    private int mCurrentMaxViewHeight;
    private ThinWebView mThinWebView;
    private BottomSheetToolbarView mToolbarView;
    private PropertyModel mToolbarModel;
    private PropertyModelChangeProcessor mModelChangeProcessor;
    private final IntentRequestTracker mIntentRequestTracker;

    /**
     * Creates a new instance.
     * @param context current {@link Context} intsance.
     * @param windowAndroid app's Adnroid window.
     * @param bottomSheetController {@BottomSheetController} instance.
     * @param tabSupplier provider to obtain {@link Tab}.
     * @param layoutView decor view.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     * @param profileSupplier Supplier of {@link Profile} for which favicon service is used.
     */
    public MerchantTrustBottomSheetCoordinator(
            Context context,
            WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController,
            Supplier<Tab> tabSupplier,
            View layoutView,
            MerchantTrustMetrics metrics,
            IntentRequestTracker intentRequestTracker,
            ObservableSupplier<Profile> profileSupplier) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mLayoutView = layoutView;
        mMetrics = metrics;
        mIntentRequestTracker = intentRequestTracker;

        mMediator =
                new MerchantTrustBottomSheetMediator(
                        context, windowAndroid, metrics, profileSupplier, new FaviconHelper());
    }

    /** Displays the details tab sheet. */
    public void requestOpenSheet(GURL url, String title, Runnable onBottomSheetDismissed) {
        setupSheet(onBottomSheetDismissed);
        mMediator.navigateToUrl(url, title);
        mBottomSheetController.requestShowContent(mSheetContent, true);
    }

    /** Closes the bottom sheet. */
    void closeSheet() {
        mBottomSheetController.hideContent(mSheetContent, true);
    }

    private void setupSheet(Runnable onBottomSheetDismissed) {
        if (mSheetContent != null) {
            return;
        }

        createToolbarView();
        createThinWebView();

        View toolbarView = mToolbarView.getView();
        ViewTreeObserver observer = toolbarView.getViewTreeObserver();
        observer.addOnPreDrawListener(
                new ViewTreeObserver.OnPreDrawListener() {
                    @Override
                    public boolean onPreDraw() {
                        toolbarView.getViewTreeObserver().removeOnPreDrawListener(this);
                        setThinWebViewLayout();
                        return true;
                    }
                });

        mMediator.setupSheetWebContents(mThinWebView, mToolbarModel);
        mSheetContent =
                new MerchantTrustBottomSheetContent(
                        mToolbarView.getView(),
                        mThinWebView.getView(),
                        () -> mMediator.getVerticalScrollOffset(),
                        this::closeSheet);

        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    private int mCloseReason;

                    @Override
                    public void onSheetContentChanged(BottomSheetContent newContent) {
                        if (newContent != mSheetContent) {
                            mMetrics.recordMetricsForBottomSheetClosed(mCloseReason);
                            if (onBottomSheetDismissed != null
                                    && (mCloseReason == StateChangeReason.NONE
                                            || mCloseReason == StateChangeReason.SWIPE
                                            || mCloseReason == StateChangeReason.BACK_PRESS
                                            || mCloseReason == StateChangeReason.TAP_SCRIM)) {
                                onBottomSheetDismissed.run();
                            }
                            destroySheet();
                        }
                    }

                    @Override
                    public void onSheetOpened(@StateChangeReason int reason) {
                        mMetrics.recordMetricsForBottomSheetHalfOpened();
                    }

                    @Override
                    public void onSheetStateChanged(int newState, int reason) {
                        if (mSheetContent == null) return;
                        switch (newState) {
                            case SheetState.PEEK:
                                mMetrics.recordMetricsForBottomSheetPeeked();
                                break;
                            case SheetState.HALF:
                                mMetrics.recordMetricsForBottomSheetHalfOpened();
                                break;
                            case SheetState.FULL:
                                mMetrics.recordMetricsForBottomSheetFullyOpened();
                                break;
                        }
                    }

                    @Override
                    public void onSheetClosed(int reason) {
                        mCloseReason = reason;
                    }
                };
        mBottomSheetController.addObserver(mBottomSheetObserver);

        mLayoutView.addOnLayoutChangeListener(this);
    }

    @VisibleForTesting
    void destroySheet() {
        mLayoutView.removeOnLayoutChangeListener(this);
        if (mBottomSheetObserver != null) {
            mBottomSheetController.removeObserver(mBottomSheetObserver);
        }
        closeSheet();
        if (mSheetContent != null) {
            mSheetContent.destroy();
        }
        mSheetContent = null;
        mMediator.destroyWebContents();
        if (mThinWebView != null) {
            mThinWebView.destroy();
        }
        mThinWebView = null;
        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
        }
        mToolbarModel = null;
        mToolbarView = null;
    }

    private void createThinWebView() {
        mThinWebView =
                ThinWebViewFactory.create(
                        mContext, new ThinWebViewConstraints(), mIntentRequestTracker);
        setThinWebViewLayout();
    }

    private void setThinWebViewLayout() {
        int height =
                (int) (getMaxViewHeight() * MerchantTrustBottomSheetContent.FULL_HEIGHT_RATIO)
                        - mToolbarView.getToolbarHeightPx();
        mThinWebView
                .getView()
                .setLayoutParams(
                        new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, height));
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) mThinWebView.getView().getLayoutParams();
        params.topMargin = mToolbarView.getToolbarHeightPx();
    }

    private void createToolbarView() {
        mToolbarView = new BottomSheetToolbarView(mContext);
        mToolbarModel =
                new PropertyModel.Builder(BottomSheetToolbarProperties.ALL_KEYS)
                        .with(
                                BottomSheetToolbarProperties.CLOSE_BUTTON_ON_CLICK_CALLBACK,
                                this::closeSheet)
                        .with(
                                BottomSheetToolbarProperties.FAVICON_ICON,
                                R.drawable.ic_logo_googleg_24dp)
                        .with(BottomSheetToolbarProperties.FAVICON_ICON_VISIBLE, true)
                        .with(BottomSheetToolbarProperties.OPEN_IN_NEW_TAB_VISIBLE, false)
                        .build();
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mToolbarModel, mToolbarView, BottomSheetToolbarViewBinder::bind);
    }

    // Returns the maximum bottom view height.
    private int getMaxViewHeight() {
        return mBottomSheetController.getContainerHeight();
    }

    @Override
    public void onLayoutChange(
            View view,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        if (mSheetContent == null) return;

        int maxViewHeight = getMaxViewHeight();
        if (maxViewHeight == 0 || mCurrentMaxViewHeight == maxViewHeight) return;
        ViewGroup.LayoutParams layoutParams = mThinWebView.getView().getLayoutParams();
        // This should never be more than the tab height for it to function correctly.
        // We scale it by |FULL_HEIGHT_RATIO| to make the size equal to that of
        // ThinWebView and so it can leave a portion of the page below it visible.
        layoutParams.height =
                (int) (maxViewHeight * MerchantTrustBottomSheetContent.FULL_HEIGHT_RATIO)
                        - mToolbarView.getToolbarHeightPx();
        ViewUtils.requestLayout(
                mThinWebView.getView(), "MerchantTrustBottomSheetCoordinator.onLayoutChange");
        mCurrentMaxViewHeight = maxViewHeight;
    }

    void setMediatorForTesting(MerchantTrustBottomSheetMediator mediator) {
        mMediator = mediator;
    }
}
