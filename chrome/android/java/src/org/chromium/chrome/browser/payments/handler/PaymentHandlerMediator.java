// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import android.app.Activity;
import android.os.Handler;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlay_panel.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.payments.ServiceWorkerPaymentAppBridge;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator.PaymentHandlerUiObserver;
import org.chromium.chrome.browser.payments.handler.toolbar.PaymentHandlerToolbarCoordinator.PaymentHandlerToolbarObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.payments.SslValidityChecker;
import org.chromium.components.payments.ui.InputProtector;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.payments.mojom.PaymentEventResponseType;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * PaymentHandler mediator, which is responsible for receiving events from the view and notifies the
 * backend (the coordinator).
 */
@NullMarked
/* package */ class PaymentHandlerMediator extends WebContentsObserver
        implements BottomSheetObserver, PaymentHandlerToolbarObserver {
    // The value is picked in order to allow users to see the tab behind this UI.
    /* package */ static final float FULL_HEIGHT_RATIO = 0.9f;
    /* package */ static final float HALF_HEIGHT_RATIO = 0.5f;

    // The height of the bottom sheet's drag handlebar: 6dp padding plus the 4dp drag_handlebar in
    // bottom_sheet.xml. Must be kept in sync with that layout.
    // TODO(crbug.com/582307863): Get the handlebar height from the bottom sheet instead. The handle
    // bar height is also different on android desktop.
    private static final float HANDLEBAR_HEIGHT_DP = 10f;

    // The minimum height of the payment app's web view when it has a preferred height, so that
    // a too small request still leaves room for a select and a pay button. Matches desktop's
    // kPaymentHandlerMinDialogHeight (196) minus its header (48) and progress bar (2).
    private static final float MIN_CONTENT_HEIGHT_DP = 146f;

    private final PropertyModel mModel;
    // Whenever invoked, invoked outside of the WebContentsObserver callbacks.
    private final Runnable mHider;
    private final WebContents mPaymentRequestWebContents;
    private final WebContents mPaymentHandlerWebContents;
    private final PaymentHandlerUiObserver mPaymentHandlerUiObserver;
    // Used to postpone execution of a callback to avoid destroy objects (e.g., WebContents) in
    // their own methods.
    private final Handler mHandler = new Handler();
    private final BottomSheetController mBottomSheetController;
    private final int mToolbarViewHeightPx;
    // The preferred content height in px: the height set by the payment app via
    // PaymentRequestEvent.openWindow(url, {preferredSize}), or the default height supplied when it
    // did not set one. Null for the legacy ServiceTabLauncher path, which uses the half/full sheet.
    private final @Nullable Integer mPreferredContentHeightPx;
    // See HANDLEBAR_HEIGHT_DP.
    private final int mHandlebarHeightPx;
    // See MIN_CONTENT_HEIGHT_DP.
    private final int mMinContentHeightPx;
    private @CloseReason int mCloseReason = CloseReason.OTHERS;
    private final ActivityStateListener mActivityStateListener;
    private final InputProtector mInputProtector;

    private boolean mIsDestroyed;

    @IntDef({
        CloseReason.OTHERS,
        CloseReason.USER,
        CloseReason.ACTIVITY_DIED,
        CloseReason.INSECURE_NAVIGATION,
        CloseReason.FAIL_LOAD
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CloseReason {
        int OTHERS = 0;
        int USER = 1;
        int ACTIVITY_DIED = 2;
        int INSECURE_NAVIGATION = 3;
        int FAIL_LOAD = 4;
    }

    /**
     * Build a new mediator that handle events from outside the payment handler component.
     *
     * @param model The {@link PaymentHandlerProperties} that holds all the view state for the
     *     payment handler component.
     * @param hider The callback to clean up {@link PaymentHandlerCoordinator} when the sheet is
     *     hidden.
     * @param paymentRequestWebContents The WebContents of the merchant's frame.
     * @param paymentHandlerWebContents The WebContents of the payment handler.
     * @param observer The {@link PaymentHandlerUiObserver} that observes this Payment Handler UI.
     * @param bottomSheetController The controller of the bottom sheet that shows this UI.
     * @param toolbarViewHeightPx The height of the toolbar view in px.
     * @param preferredContentHeightPx The preferred content height in px: the height set by the
     *     payment app, or the default height supplied when it did not set one. Null for the legacy
     *     ServiceTabLauncher path, which uses the half/full sheet.
     * @param activity The current android {@link Activity}.
     */
    /* package */ PaymentHandlerMediator(
            PropertyModel model,
            Runnable hider,
            WebContents paymentRequestWebContents,
            WebContents paymentHandlerWebContents,
            PaymentHandlerUiObserver observer,
            BottomSheetController bottomSheetController,
            int toolbarViewHeightPx,
            @Nullable Integer preferredContentHeightPx,
            Activity activity,
            InputProtector inputProtector) {
        super(paymentHandlerWebContents);
        assert paymentHandlerWebContents != null;
        mBottomSheetController = bottomSheetController;
        mPaymentRequestWebContents = paymentRequestWebContents;
        mPaymentHandlerWebContents = paymentHandlerWebContents;
        mToolbarViewHeightPx = toolbarViewHeightPx;
        mPreferredContentHeightPx = preferredContentHeightPx;
        mHandlebarHeightPx = ViewUtils.dpToPx(activity, HANDLEBAR_HEIGHT_DP);
        mMinContentHeightPx = ViewUtils.dpToPx(activity, MIN_CONTENT_HEIGHT_DP);
        mModel = model;
        mModel.set(PaymentHandlerProperties.BACK_PRESS_CALLBACK, this::onSystemBackButtonClicked);
        mHider = hider;
        mPaymentHandlerUiObserver = observer;
        // The height is not set here on purpose. The sheet has not been laid out yet, so any value
        // read now would be wrong. It is set once the sheet settles into a state.
        mInputProtector = inputProtector;

        mActivityStateListener =
                (Activity _, int newState) -> {
                    if (newState == ActivityState.DESTROYED) {
                        mCloseReason = CloseReason.ACTIVITY_DIED;
                        mHider.run();
                    }
                };
        ApplicationStatus.registerStateListenerForActivity(mActivityStateListener, activity);
    }

    /** Destroy the dependencies of the Mediator. */
    public void destroy() {
        if (mIsDestroyed) return;
        mIsDestroyed = true;

        observe(null);

        ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);

        switch (mCloseReason) {
            case CloseReason.INSECURE_NAVIGATION:
                ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(
                        mPaymentRequestWebContents,
                        PaymentEventResponseType.PAYMENT_HANDLER_INSECURE_NAVIGATION);
                break;
            case CloseReason.USER:
                ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(
                        mPaymentRequestWebContents,
                        PaymentEventResponseType.PAYMENT_HANDLER_WINDOW_CLOSING);
                break;
            case CloseReason.FAIL_LOAD:
                ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(
                        mPaymentRequestWebContents,
                        PaymentEventResponseType.PAYMENT_HANDLER_FAIL_TO_LOAD_MAIN_FRAME);
                break;
            case CloseReason.ACTIVITY_DIED:
                ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(
                        mPaymentRequestWebContents,
                        PaymentEventResponseType.PAYMENT_HANDLER_ACTIVITY_DIED);
                break;
            case CloseReason.OTHERS:
                // No need to notify ServiceWorkerPaymentAppBridge when merchant aborts the
                // payment request (and thus {@link ChromePaymentRequestService} closes
                // PaymentHandlerMediator). "OTHERS" category includes this cases.
                // TODO(crbug.com/40134410): we should explicitly list merchant aborting payment
                // request as a {@link CloseReason}, renames "OTHERS" as "UNKNOWN" and asserts
                // that PaymentHandler wouldn't be closed for unknown reason.
        }
        mHandler.removeCallbacksAndMessages(null);
    }

    // Implement BottomSheetObserver:
    @Override
    public void onSheetStateChanged(@SheetState int newState, int reason) {
        switch (newState) {
            case SheetState.HIDDEN:
                mCloseReason = CloseReason.USER;
                mHandler.post(mHider);
                break;
            default:
                // The sheet's own measurements are only trustworthy once it has settled into a
                // state.
                updateContentVisibleHeight();
                break;
        }
    }

    /**
     * Pushes the height the payment app's web view should have to the view. This is the height of
     * the sheet's content area when the sheet is fully expanded. It does not change when the user
     * drags the sheet between half and full, because the sheet moves rather than resizes. With a
     * preferred height, it is the clamped preferred height instead; see
     * computeWrapContentWebViewHeightPx().
     */
    private void updateContentVisibleHeight() {
        int visibleHeightPx = contentVisibleHeight();
        if (visibleHeightPx <= 0) return;
        mModel.set(PaymentHandlerProperties.CONTENT_VISIBLE_HEIGHT_PX, visibleHeightPx);
    }

    /**
     * @return The height of visible area of the bottom sheet's content part.
     */
    private int contentVisibleHeight() {
        if (mPreferredContentHeightPx != null) {
            return computeWrapContentWebViewHeightPx(
                    mPreferredContentHeightPx,
                    mMinContentHeightPx,
                    mToolbarViewHeightPx,
                    mHandlebarHeightPx,
                    mBottomSheetController.getMaxSheetHeight());
        }
        return mBottomSheetController.getMaxOffset() - mToolbarViewHeightPx;
    }

    /**
     * Whether the sheet has a preferred height and wraps its content: either the payment app set
     * one via PaymentRequestEvent.openWindow(url, {preferredSize}), or a default height was
     * supplied because it did not. False for the legacy ServiceTabLauncher path, which uses the
     * half/full sheet.
     */
    private boolean hasPreferredHeight() {
        return mPreferredContentHeightPx != null;
    }

    /**
     * Returns the preferred height, raised to the minimum content height, then clamped so that the
     * whole sheet (handlebar, toolbar and web view) fits within the full height ratio of the
     * maximum sheet height. The available space wins over the minimum, so the sheet never overflows
     * a small window.
     *
     * @param preferredContentHeightPx The preferred content height, in pixels: the height set by
     *     the payment app, or the default height supplied when it did not set one.
     * @param minContentHeightPx The minimum content height, in pixels.
     * @param toolbarHeightPx The height of the Payment Handler toolbar, in pixels.
     * @param handlebarHeightPx The height of the bottom sheet's handlebar, in pixels.
     * @param maxSheetHeightPx The maximum height of the bottom sheet, in pixels, or a non-positive
     *     value if unknown.
     * @return The web view height in pixels, or a non-positive value if it can't be computed yet.
     */
    @VisibleForTesting
    /* package */ static int computeWrapContentWebViewHeightPx(
            int preferredContentHeightPx,
            int minContentHeightPx,
            int toolbarHeightPx,
            int handlebarHeightPx,
            int maxSheetHeightPx) {
        int availablePx =
                (int) (FULL_HEIGHT_RATIO * maxSheetHeightPx) - toolbarHeightPx - handlebarHeightPx;
        return Math.min(availablePx, Math.max(minContentHeightPx, preferredContentHeightPx));
    }

    // Implement BottomSheetObserver:
    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {}

    // Implement BottomSheetObserver:
    @Override
    public void onSheetOpened(@StateChangeReason int reason) {
        mPaymentHandlerUiObserver.onPaymentHandlerUiShown();
    }

    // Implement BottomSheetObserver:
    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        // This is invoked when the sheet returns to the peek state, but Payment Handler doesn't
        // have a peek state.
    }

    // Implement BottomSheetObserver:
    @Override
    public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
        // Size the web view to the preferred height as soon as the sheet shows this content, i.e.
        // right after requestShowContent(). The sheet's max height is only known from this point on
        // the first show, since the bottom sheet is created lazily in requestShowContent().
        if (hasPreferredHeight()) updateContentVisibleHeight();
    }

    // Implement BottomSheetObserver:
    @Override
    public void onContainerSizeChanged(int newWidth, int newHeight) {
        // TODO(crbug.com/582307771): Re-clamp the preferred height when the container size
        // changes, e.g. on rotation.
    }

    // Implement WebContentsObserver:
    @Override
    public void webContentsDestroyed() {
        destroy();
    }

    // Implement WebContentsObserver:
    @Override
    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigationHandle) {
        // Checking uncommitted navigations (e.g., Network errors) is unnecessary because
        // they have no chance to be loaded nor rendered.
        if (navigationHandle.isSameDocument() || !navigationHandle.hasCommitted()) {
            return;
        }
        closeIfInsecure();
    }

    // Implement WebContentsObserver:
    @Override
    public void didChangeVisibleSecurityState() {
        closeIfInsecure();
    }

    private void closeIfInsecure() {
        if (!SslValidityChecker.isValidPageInPaymentHandlerWindow(mPaymentHandlerWebContents)) {
            closeUiForInsecureNavigation();
        }
    }

    private void closeUiForInsecureNavigation() {
        mHandler.post(
                () -> {
                    mCloseReason = CloseReason.INSECURE_NAVIGATION;
                    mHider.run();
                });
    }

    // Implement WebContentsObserver:
    @Override
    public void didFailLoad(
            boolean isInPrimaryMainFrame,
            int errorCode,
            GURL failingUrl,
            @LifecycleState int rfhLifecycleState) {
        if (!isInPrimaryMainFrame) return;
        mHandler.post(
                () -> {
                    mCloseReason = CloseReason.FAIL_LOAD;
                    mHider.run();
                });
    }

    // Implement PaymentHandlerToolbarObserver:
    @Override
    public void onToolbarCloseButtonClicked() {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        mCloseReason = CloseReason.USER;
        mHandler.post(mHider);
    }

    private void onSystemBackButtonClicked() {
        NavigationController navigation = mPaymentHandlerWebContents.getNavigationController();
        if (navigation != null && navigation.canGoBack()) navigation.goBack();
    }
}
