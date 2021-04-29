// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.merchant_viewer;

import android.content.Context;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.version.ChromeVersionInfo;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Coordinator for managing the merchant trust details page experience. */
public class MerchantTrustDetailsTabCoordinator implements View.OnLayoutChangeListener {
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final BottomSheetController mBottomSheetController;
    private final Supplier<Tab> mTabSupplier;
    private final View mLayoutView;
    private final MerchantTrustDetailsTabMediator mMediator;
    private final MerchantTrustMetrics mMetrics;

    private WebContents mWebContents;
    private ContentView mWebContentView;
    private BottomSheetObserver mBottomSheetObserver;
    private MerchantTrustDetailsSheetContent mSheetContent;
    private int mCurrentMaxViewHeight;

    /**
     * Creates a new instance.
     * @param context current {@link Context} intsance.
     * @param windowAndroid app's Adnroid window.
     * @param bottomSheetController {@BottomSheetController} instance.
     * @param tabSupplier provider to obtain {@link Tab}.
     * @param layoutView decor view.
     */
    public MerchantTrustDetailsTabCoordinator(Context context, WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController, Supplier<Tab> tabSupplier, View layoutView,
            MerchantTrustMetrics metrics) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mTabSupplier = tabSupplier;
        mBottomSheetController = bottomSheetController;
        mLayoutView = layoutView;
        mMetrics = metrics;

        float topControlsHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                / mWindowAndroid.getDisplay().getDipScale();
        mMediator = new MerchantTrustDetailsTabMediator(
                mBottomSheetController, (int) topControlsHeight, mMetrics);
    }

    /** Displays the details tab sheet. */
    public void requestOpenSheet(GURL url, String title) {
        Profile profile = Profile.getLastUsedRegularProfile();
        setupSheetWebContentsIfNeeded(profile);
        mMediator.requestShowContent(url, title);
    }

    /** Closes the bottom sheet. */
    void close() {
        mBottomSheetController.hideContent(mSheetContent, true);
    }

    private void createWebContents(Profile profile) {
        assert mWebContents == null;
        mWebContents = WebContentsHelpers.createWebContents(false, false);
        mWebContentView = ContentView.createContentView(mContext, null, mWebContents);
        final ViewAndroidDelegate delegate =
                ViewAndroidDelegate.createBasicDelegate(mWebContentView);
        mWebContents.initialize(ChromeVersionInfo.getProductVersion(), delegate, mWebContentView,
                mWindowAndroid, WebContents.createDefaultInternalsHolder());
        WebContentsHelpers.setUserAgentOverride(mWebContents);
    }

    // Calculates the maximum view height based on the height of the tab provided by mTabSupplier.
    private int getMaxViewHeight() {
        final Tab tab = mTabSupplier.get();
        if (tab == null || tab.getView() == null) return 0;
        return tab.getView().getHeight();
    }

    private void destroyWebContents() {
        mSheetContent = null;

        if (mWebContents != null) {
            mWebContents.destroy();
            mWebContents = null;
            mWebContentView = null;
        }

        mMediator.destroyContent();
        mLayoutView.removeOnLayoutChangeListener(this);
        if (mBottomSheetObserver != null) {
            mBottomSheetController.removeObserver(mBottomSheetObserver);
        }
    }

    private void setupSheetWebContentsIfNeeded(Profile profile) {
        if (mWebContents != null) {
            return;
        }

        assert mSheetContent == null;
        createWebContents(profile);

        // TODO: Observe changes and log metrics.
        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            private int mCloseReason;

            @Override
            public void onSheetContentChanged(BottomSheetContent newContent) {
                if (newContent != mSheetContent) {
                    mMetrics.recordMetricsForBottomSheetClosed(mCloseReason);
                    destroyWebContents();
                }
            }

            @Override
            public void onSheetOpened(@StateChangeReason int reason) {
                mMetrics.recordMetricsForBottomSheetHalfOpened();
            }

            @Override
            public void onSheetStateChanged(int newState) {
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
        mSheetContent =
                new MerchantTrustDetailsSheetContent(mContext, this::close, this::getMaxViewHeight);
        mMediator.init(mWebContents, mWebContentView, mSheetContent, profile);
        mLayoutView.addOnLayoutChangeListener(this);
    }

    @Override
    public void onLayoutChange(View view, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        if (mSheetContent == null) return;

        int maxViewHeight = getMaxViewHeight();
        if (maxViewHeight == 0 || mCurrentMaxViewHeight == maxViewHeight) return;
        mSheetContent.updateContentHeight(maxViewHeight);
        mCurrentMaxViewHeight = maxViewHeight;
    }
}