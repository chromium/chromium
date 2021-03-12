// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import android.os.Handler;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.payments.ServiceWorkerPaymentAppBridge;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator.PaymentHandlerUiObserver;
import org.chromium.chrome.browser.payments.handler.toolbar.PaymentHandlerToolbarCoordinator.PaymentHandlerToolbarObserver;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.payments.SslValidityChecker;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.payments.mojom.PaymentEventResponseType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * PaymentHandler mediator, which is responsible for receiving events from the view and notifies the
 * backend (the coordinator).
 */
/* package */ class PaymentHandlerMediator extends WebContentsObserver
        implements BottomSheetObserver, PaymentHandlerToolbarObserver, View.OnLayoutChangeListener {
    // The value is picked in order to allow users to see the tab behind this UI.
    /* package */ static final float FULL_HEIGHT_RATIO = 0.9f;
    /* package */ static final float HALF_HEIGHT_RATIO = 0.5f;

    private final PropertyModel mModel;
    // Whenever invoked, invoked outside of the WebContentsObserver callbacks.
    private final Runnable mHider;
    private final WebContents mPaymentRequestWebContents;
    private final WebContents mPaymentHandlerWebContents;
    private final PaymentHandlerUiObserver mPaymentHandlerUiObserver;
    // Used to postpone execution of a callback to avoid destroy objects (e.g., WebContents) in
    // their own methods.
    private final Handler mHandler = new Handler();
    private final Destroyable mActivityDestroyListener;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final View mTabView;
    private final BottomSheetController mBottomSheetController;
    private final int mToolbarViewHeightPx;
    private @CloseReason int mCloseReason = CloseReason.OTHERS;

    /** A token held while the payment sheet is obscuring all visible tabs. */
    private int mTabObscuringToken = TokenHolder.INVALID_TOKEN;

    @IntDef({CloseReason.OTHERS, CloseReason.USER, CloseReason.ACTIVITY_DIED,
            CloseReason.INSECURE_NAVIGATION, CloseReason.FAIL_LOAD})
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
     * @param model The {@link PaymentHandlerProperties} that holds all the view state for the
     *         payment handler component.
     * @param hider The callback to clean up {@link PaymentHandlerCoordinator} when the sheet is
     *         hidden.
     * @param paymentRequestWebContents The WebContents of the merchant's frame.
     * @param paymentHandlerWebContents The WebContents of the payment handler.
     * @param observer The {@link PaymentHandlerUiObserver} that observes this Payment Handler UI.
     * @param tabView The view of the main tab.
     * @param toolbarViewHeightPx The height of the toolbar view in px.
     * @param activityLifeCycleDispatcher The lifecycle dispatcher of the activity where this UI
     *         lives.
     * @param sheetController A {@link BottomSheetController} to show UI in.
     */
    /* package */ PaymentHandlerMediator(PropertyModel model, Runnable hider,
            WebContents paymentRequestWebContents, WebContents paymentHandlerWebContents,
            PaymentHandlerUiObserver observer, View tabView, int toolbarViewHeightPx,
            ActivityLifecycleDispatcher activityLifeCycleDispatcher,
            BottomSheetController sheetController) {
        super(paymentHandlerWebContents);
        assert paymentHandlerWebContents != null;
        mTabView = tabView;
        mBottomSheetController = sheetController;
        mPaymentRequestWebContents = paymentRequestWebContents;
        mPaymentHandlerWebContents = paymentHandlerWebContents;
        mToolbarViewHeightPx = toolbarViewHeightPx;
        mModel = model;
        mModel.set(PaymentHandlerProperties.BACK_PRESS_CALLBACK, this::onSystemBackButtonClicked);
        mHider = hider;
        mPaymentHandlerUiObserver = observer;
        mModel.set(PaymentHandlerProperties.CONTENT_VISIBLE_HEIGHT_PX, contentVisibleHeight());

        mActivityLifecycleDispatcher = activityLifeCycleDispatcher;
        mActivityDestroyListener = new Destroyable() {
            @Override
            public void destroy() {
                mCloseReason = CloseReason.ACTIVITY_DIED;
                mHandler.post(mHider);
            }
        };
        mActivityLifecycleDispatcher.register(mActivityDestroyListener);
    }

    // Implement View.OnLayoutChangeListener:
    // This is the Tab View's layout change listener, invoked in response to phone rotation.
    // TODO(crbug.com/1057825): It should listen to the BottomSheet container's layout change
    // instead of the Tab View layout change for better encapsulation.
    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        mModel.set(PaymentHandlerProperties.CONTENT_VISIBLE_HEIGHT_PX, contentVisibleHeight());
    }

    // Implement BottomSheetObserver:
    @Override
    public void onSheetStateChanged(@SheetState int newState) {
        switch (newState) {
            case BottomSheetController.SheetState.HIDDEN:
                mCloseReason = CloseReason.USER;
                mHandler.post(mHider);
                break;
        }
    }

    /** @return The height of visible area of the bottom sheet's content part. */
    private int contentVisibleHeight() {
        return (int) (mTabView.getHeight() * FULL_HEIGHT_RATIO) - mToolbarViewHeightPx;
    }

    // Implement BottomSheetObserver:
    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {}

    /**
     * Set whether to obscure all tabs. Note the difference between scrim and obscure, while scrims
     * reduces the background visibility, obscure makes the background invisible to screen readers.
     * @param activity The ChromeActivity of the tab.
     * @param obscure Whether to obscure all tabs.
     */
    private void setObscureState(ChromeActivity activity, boolean obscure) {
        TabObscuringHandler obscuringHandler = activity.getTabObscuringHandler();
        if (obscuringHandler == null) return;

        if (obscure && mTabObscuringToken == TokenHolder.INVALID_TOKEN) {
            mTabObscuringToken = obscuringHandler.obscureAllTabs();
        } else if (!obscure && mTabObscuringToken != TokenHolder.INVALID_TOKEN) {
            obscuringHandler.unobscureAllTabs(mTabObscuringToken);
            mTabObscuringToken = TokenHolder.INVALID_TOKEN;
        }
    }

    private void showScrim() {
        // Using an empty scrim observer is to avoid the dismissal of the bottom-sheet on tapping.
        ChromeActivity activity = ChromeActivity.fromWebContents(mPaymentHandlerWebContents);
        assert activity != null;

        ScrimCoordinator coordinator = mBottomSheetController.getScrimCoordinator();
        if (coordinator != null && !coordinator.isShowingScrim()) {
            PropertyModel params = mBottomSheetController.createScrimParams();
            coordinator.showScrim(params);
        }
        setObscureState(activity, true);
    }

    // Implement BottomSheetObserver:
    @Override
    public void onSheetOpened(@StateChangeReason int reason) {
        mPaymentHandlerUiObserver.onPaymentHandlerUiShown();
        showScrim();
    }

    // Implement BottomSheetObserver:
    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        // This is invoked when the sheet returns to the peek state, but Payment Handler doesn't
        // have a peek state.
    }

    // Implement BottomSheetObserver:
    @Override
    public void onSheetFullyPeeked() {}

    // Implement BottomSheetObserver:
    @Override
    public void onSheetContentChanged(BottomSheetContent newContent) {}

    // Implement WebContentsObserver:
    @Override
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(mActivityDestroyListener);

        switch (mCloseReason) {
            case CloseReason.INSECURE_NAVIGATION:
                ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(mPaymentRequestWebContents,
                        PaymentEventResponseType.PAYMENT_HANDLER_INSECURE_NAVIGATION);
                break;
            case CloseReason.USER:
                ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(mPaymentRequestWebContents,
                        PaymentEventResponseType.PAYMENT_HANDLER_WINDOW_CLOSING);
                break;
            case CloseReason.FAIL_LOAD:
                ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(mPaymentRequestWebContents,
                        PaymentEventResponseType.PAYMENT_HANDLER_FAIL_TO_LOAD_MAIN_FRAME);
                break;
            case CloseReason.ACTIVITY_DIED:
                ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(mPaymentRequestWebContents,
                        PaymentEventResponseType.PAYMENT_HANDLER_ACTIVITY_DIED);
                break;
            case CloseReason.OTHERS:
                // No need to notify ServiceWorkerPaymentAppBridge when merchant aborts the
                // payment request (and thus {@link ChromePaymentRequestService} closes
                // PaymentHandlerMediator). "OTHERS" category includes this cases.
                // TODO(crbug.com/1091957): we should explicitly list merchant aborting payment
                // request as a {@link CloseReason}, renames "OTHERS" as "UNKNOWN" and asserts
                // that PaymentHandler wouldn't be closed for unknown reason.
        }
        mHandler.removeCallbacksAndMessages(null);
        hideScrim();
        super.destroy(); // Stops observing the web contents and cleans up associated references.
    }

    private void hideScrim() {
        ChromeActivity activity = ChromeActivity.fromWebContents(mPaymentHandlerWebContents);
        // activity would be null when this method is triggered by activity being destroyed.
        if (activity == null) return;

        setObscureState(activity, false);

        ScrimCoordinator coordinator = mBottomSheetController.getScrimCoordinator();
        if (coordinator != null && coordinator.isShowingScrim()) {
            coordinator.hideScrim(/*animate=*/true);
        }
    }

    // Implement WebContentsObserver:
    @Override
    public void didFinishNavigation(NavigationHandle navigationHandle) {
        if (navigationHandle.isSameDocument()) return;
        closeIfInsecure();
    }

    // Implement WebContentsObserver:
    @Override
    public void didChangeVisibleSecurityState() {
        closeIfInsecure();
    }

    private void closeIfInsecure() {
        if (!SslValidityChecker.isValidPageInPaymentHandlerWindow(mPaymentHandlerWebContents)) {
            closeUIForInsecureNavigation();
        }
    }

    private void closeUIForInsecureNavigation() {
        mHandler.post(() -> {
            mCloseReason = CloseReason.INSECURE_NAVIGATION;
            mHider.run();
        });
    }

    // Implement WebContentsObserver:
    @Override
    public void didFailLoad(boolean isMainFrame, int errorCode, GURL failingUrl) {
        if (!isMainFrame) return;
        mHandler.post(() -> {
            mCloseReason = CloseReason.FAIL_LOAD;
            mHider.run();
        });
    }

    // Implement PaymentHandlerToolbarObserver:
    @Override
    public void onToolbarCloseButtonClicked() {
        mCloseReason = CloseReason.USER;
        mHandler.post(mHider);
    }

    private void onSystemBackButtonClicked() {
        NavigationController navigation = mPaymentHandlerWebContents.getNavigationController();
        if (navigation != null && navigation.canGoBack()) navigation.goBack();
    }
}
