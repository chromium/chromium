// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.payments.handler.toolbar.PaymentHandlerToolbarCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

/**
 * PaymentHandler coordinator, which owns the component overall, i.e., creates other objects in the
 * component and connects them. It decouples the implementation of this component from other
 * components and acts as the point of contact between them. Any code in this component that needs
 * to interact with another component does that through this coordinator.
 */
public class PaymentHandlerCoordinator {
    private Runnable mHider;
    private WebContents mWebContents;
    private PaymentHandlerToolbarCoordinator mToolbarCoordinator;

    /** Constructs the payment-handler component coordinator. */
    public PaymentHandlerCoordinator() {
        assert isEnabled();
    }

    /** Observes the state changes of the payment-handler UI. */
    public interface PaymentHandlerUiObserver {
        /** Called when Payment Handler UI is closed. */
        void onPaymentHandlerUiClosed();
        /** Called when Payment Handler UI is shown. */
        void onPaymentHandlerUiShown();
    }

    /** Observes the WebContents of the payment-handler UI. */
    public interface PaymentHandlerWebContentsObserver {
        /**
         * Called when the WebContents has been initialized.
         * @param webContents The WebContents of the PaymentHandler.
         */
        void onWebContentsInitialized(WebContents webContents);
    }

    /**
     * Shows the payment-handler UI.
     *
     * @param activity The activity where the UI should be shown.
     * @param url The url of the payment handler app, i.e., that of
     *         "PaymentRequestEvent.openWindow(url)".
     * @param isIncognito Whether the tab is in incognito mode.
     * @param webContentsObserver The observer of the WebContents of the
     *         PaymentHandler.
     * @param uiObserver The {@link PaymentHandlerUiObserver} that observes this Payment Handler UI.
     * @return Whether the payment-handler UI was shown. Can be false if the UI was suppressed.
     */
    public boolean show(ChromeActivity activity, GURL url, boolean isIncognito,
            PaymentHandlerWebContentsObserver webContentsObserver,
            PaymentHandlerUiObserver uiObserver) {
        assert mHider == null : "Already showing payment-handler UI";

        mWebContents = WebContentsFactory.createWebContents(isIncognito, /*initiallyHidden=*/false);
        ContentView webContentView = ContentView.createContentView(
                activity, null /* eventOffsetHandler */, mWebContents);
        initializeWebContents(activity, webContentView, webContentsObserver, url);

        mToolbarCoordinator = new PaymentHandlerToolbarCoordinator(activity, mWebContents, url);

        PropertyModel model = new PropertyModel.Builder(PaymentHandlerProperties.ALL_KEYS).build();
        PaymentHandlerMediator mediator = new PaymentHandlerMediator(model, this::hide,
                mWebContents, uiObserver, activity.getActivityTab().getView(),
                mToolbarCoordinator.getToolbarHeightPx(),
                activity.getLifecycleDispatcher(),
                BottomSheetControllerProvider.from(activity.getWindowAndroid()));
        activity.getWindow().getDecorView().addOnLayoutChangeListener(mediator);
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(activity.getWindowAndroid());
        bottomSheetController.addObserver(mediator);
        mWebContents.addObserver(mediator);

        mToolbarCoordinator.setCloseButtonOnClickCallback(mediator::onToolbarCloseButtonClicked);
        ThinWebView thinWebView = ThinWebViewFactory.create(activity, new ThinWebViewConstraints());
        assert webContentView.getParent() == null;
        thinWebView.attachWebContents(mWebContents, webContentView, null);
        PaymentHandlerView view = new PaymentHandlerView(
                activity, mWebContents, mToolbarCoordinator.getView(), thinWebView.getView());
        assert mToolbarCoordinator.getToolbarHeightPx() == view.getToolbarHeightPx();
        PropertyModelChangeProcessor changeProcessor =
                PropertyModelChangeProcessor.create(model, view, PaymentHandlerViewBinder::bind);
        mHider = () -> {
            changeProcessor.destroy();
            bottomSheetController.removeObserver(mediator);
            bottomSheetController.hideContent(/*content=*/view, /*animate=*/true);
            uiObserver.onPaymentHandlerUiClosed();
            assert activity.getWindow() != null;
            assert activity.getWindow().getDecorView() != null;
            activity.getWindow().getDecorView().removeOnLayoutChangeListener(mediator);
            mediator.destroy();
            thinWebView.destroy();
            mWebContents.destroy();
        };
        return bottomSheetController.requestShowContent(view, /*animate=*/true);
    }

    private void initializeWebContents(ChromeActivity activity, ContentView webContentView,
            PaymentHandlerWebContentsObserver webContentsObserver, GURL url) {
        mWebContents.initialize(ChromeVersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(webContentView), webContentView,
                activity.getWindowAndroid(), WebContents.createDefaultInternalsHolder());

        SelectionPopupController controller =
                SelectionPopupController.fromWebContents(mWebContents);
        controller.setActionModeCallback(new PaymentHandlerActionModeCallback(mWebContents));
        controller.setSelectionClient(SelectionClient.createSmartSelectionClient(mWebContents));

        webContentsObserver.onWebContentsInitialized(mWebContents);
        mWebContents.getNavigationController().loadUrl(new LoadUrlParams(url.getSpec()));
    }

    /**
     * Get the WebContents of the Payment Handler for testing purpose. In other situations,
     * WebContents should not be leaked outside the Payment Handler.
     *
     * @return The WebContents of the Payment Handler.
     */
    @VisibleForTesting
    public WebContents getWebContentsForTest() {
        return mWebContents;
    }

    /** Hides the payment-handler UI. */
    public void hide() {
        if (mHider == null) return;
        mHider.run();
        mHider = null;
    }

    /**
     * @return Whether this solution (as opposed to the Chrome-custom-tab based solution) of
     *     PaymentHandler is enabled. This solution is intended to replace the other
     *     solution.
     */
    public static boolean isEnabled() {
        // Enabling the flag of either ScrollToExpand or PaymentsExperimentalFeatures will enable
        // this feature.
        return PaymentFeatureList.isEnabledOrExperimentalFeaturesEnabled(
                PaymentFeatureList.SCROLL_TO_EXPAND_PAYMENT_HANDLER);
    }

    @VisibleForTesting
    public void clickSecurityIconForTest() {
        mToolbarCoordinator.clickSecurityIconForTest();
    }
}
