// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.payments.InputProtector;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;

/**
 * The view of the PaymentHandler UI. This view can be divided into the toolbar area and the
 * content area. The content area does not include the toolbar area; it includes the BottomSheet
 * area below the toolbar, which includes the part that extends beneath the screen. The ThinWebView
 * has a fixed height, which is the height of visible content area when the sheet is in full state.
 */
/* package */ class PaymentHandlerView implements BottomSheetContent {
    private final View mToolbarView;
    private final PaymentHandlerContentFrameLayout mContentView;
    private final View mThinWebView;
    private final WebContents mWebContents;
    private final int mToolbarHeightPx;
    private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
            new ObservableSupplierImpl<>();
    private Runnable mBackPressCallback;

    /**
     * Construct the PaymentHandlerView.
     *
     * @param context The {@link Context} of the Application.
     * @param webContents The web-content of the payment-handler web-app.
     * @param toolbarView The view of the Payment Handler toolbar.
     * @param thinWebView The view that shows the WebContents of the payment app.
     */
    /* package */ PaymentHandlerView(
            Context context,
            WebContents webContents,
            View toolbarView,
            View thinWebView,
            InputProtector inputProtector) {
        mWebContents = webContents;
        mToolbarView = toolbarView;
        mThinWebView = thinWebView;
        mToolbarHeightPx =
                context.getResources().getDimensionPixelSize(R.dimen.sheet_tab_toolbar_height);
        mContentView =
                (PaymentHandlerContentFrameLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.payment_handler_content, null);
        mContentView.setInputProtector(inputProtector);
        mContentView.setPadding(
                /* left= */ 0, /* top= */ mToolbarHeightPx, /* right= */ 0, /* bottom= */ 0);
        mContentView.addView(thinWebView, /* index= */ 0);
        mBackPressStateChangedSupplier.set(true);
    }

    /**
     * Set the callback to be invoked when the system back button is pressed.
     * @param callback the callback to be invoked
     */
    /* package */ void setBackPressCallback(Runnable callback) {
        mBackPressCallback = callback;
    }

    /**
     * Invoked when the visible area of the content container changes.
     * @param heightPx The height of the visible area of the Payment Handler UI content container,
     *         in pixels.
     */
    /* package */ void onContentVisibleHeightChanged(int heightPx) {
        LayoutParams params = (LayoutParams) mThinWebView.getLayoutParams();
        params.height = Math.max(0, heightPx);
        mThinWebView.setLayoutParams(params);
    }

    /* package */ int getToolbarHeightPx() {
        return mToolbarHeightPx;
    }

    // BottomSheetContent:
    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public @Nullable View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public float getFullHeightRatio() {
        return PaymentHandlerMediator.FULL_HEIGHT_RATIO;
    }

    @Override
    public float getHalfHeightRatio() {
        return PaymentHandlerMediator.HALF_HEIGHT_RATIO;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        return true;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mWebContents == null
                ? 0
                : RenderCoordinates.fromWebContents(mWebContents).getScrollYPixInt();
    }

    @Override
    public void destroy() {}

    @Override
    public @ContentPriority int getPriority() {
        // If multiple bottom sheets are queued up to be shown, prioritize payment-handler, because
        // it's triggered by a user gesture, such as a click on <button>Buy this article</button>.
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public int getPeekHeight() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public boolean handleBackPress() {
        mBackPressCallback.run();
        return true; // Prevent further handling of the back press.
    }

    @Override
    public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
        return mBackPressStateChangedSupplier;
    }

    @Override
    public void onBackPressed() {
        mBackPressCallback.run();
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.payment_handler_sheet_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.payment_handler_sheet_opened_half;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.payment_handler_sheet_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.payment_handler_sheet_closed;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        // flinging down hard enough will close the sheet.
        return true;
    }
}
