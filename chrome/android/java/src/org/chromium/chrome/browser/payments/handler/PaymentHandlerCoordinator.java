// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import android.app.Activity;

import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.payments.handler.toolbar.PaymentHandlerToolbarCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabObscuringHandlerSupplier;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.payments.InputProtector;
import org.chromium.components.payments.PaymentHandlerNavigationThrottle;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
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
    private WebContents mPaymentHandlerWebContents;
    private PaymentHandlerToolbarCoordinator mToolbarCoordinator;
    private InputProtector mInputProtector = new InputProtector();

    /** Constructs the payment-handler component coordinator. */
    public PaymentHandlerCoordinator() {}

    /** Observes the state changes of the payment-handler UI. */
    public interface PaymentHandlerUiObserver {
        /** Called when Payment Handler UI is closed. */
        void onPaymentHandlerUiClosed();

        /** Called when Payment Handler UI is shown. */
        void onPaymentHandlerUiShown();
    }

    /**
     * Shows the payment-handler UI.
     *
     * @param paymentRequestWebContents The WebContents of the merchant's frame.
     * @param url The url of the payment handler app, i.e., that of
     *     "PaymentRequestEvent.openWindow(url)".
     * @param uiObserver The {@link PaymentHandlerUiObserver} that observes this Payment Handler UI.
     * @return The WebContents of the payment handler that's just opened when the showing is
     *     successful; null if failed. When null is returned, caller should also call hide().
     */
    public WebContents show(
            WebContents paymentRequestWebContents, GURL url, PaymentHandlerUiObserver uiObserver) {
        assert mHider == null : "Already showing payment-handler UI";
        assert paymentRequestWebContents != null;
        WindowAndroid windowAndroid = paymentRequestWebContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return null;
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) return null;
        Profile profile = Profile.fromWebContents(paymentRequestWebContents);
        if (profile == null) return null;

        mInputProtector.markShowTime();
        mPaymentHandlerWebContents =
                WebContentsFactory.createWebContents(profile, /* initiallyHidden= */ false, false);
        PaymentHandlerNavigationThrottle.markPaymentHandlerWebContents(mPaymentHandlerWebContents);
        ContentView webContentView =
                ContentView.createContentView(activity, mPaymentHandlerWebContents);
        initializeWebContents(windowAndroid, webContentView, url);

        mToolbarCoordinator =
                new PaymentHandlerToolbarCoordinator(
                        activity,
                        mPaymentHandlerWebContents,
                        url,
                        windowAndroid::getModalDialogManager);

        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        Tab currentTab = TabModelSelectorSupplier.getCurrentTabFrom(windowAndroid);
        TabObscuringHandler tabObscuringHandler =
                TabObscuringHandlerSupplier.getValueOrNullFrom(windowAndroid);
        if (bottomSheetController == null || currentTab == null || tabObscuringHandler == null) {
            return null;
        }

        PropertyModel model = new PropertyModel.Builder(PaymentHandlerProperties.ALL_KEYS).build();
        PaymentHandlerMediator mediator =
                new PaymentHandlerMediator(
                        model,
                        this::hide,
                        /* paymentRequestWebContents= */ paymentRequestWebContents,
                        /* paymentHandlerWebContents= */ mPaymentHandlerWebContents,
                        uiObserver,
                        currentTab.getView(),
                        mToolbarCoordinator.getToolbarHeightPx(),
                        bottomSheetController,
                        tabObscuringHandler,
                        activity,
                        mInputProtector);
        activity.getWindow().getDecorView().addOnLayoutChangeListener(mediator);

        bottomSheetController.addObserver(mediator);
        mPaymentHandlerWebContents.addObserver(mediator);

        mToolbarCoordinator.setCloseButtonOnClickCallback(mediator::onToolbarCloseButtonClicked);
        IntentRequestTracker intentRequestTracker = windowAndroid.getIntentRequestTracker();
        assert intentRequestTracker != null;
        ThinWebView thinWebView =
                ThinWebViewFactory.create(
                        activity, new ThinWebViewConstraints(), intentRequestTracker);
        assert webContentView.getParent() == null;
        thinWebView.attachWebContents(mPaymentHandlerWebContents, webContentView, null);
        PaymentHandlerView view =
                new PaymentHandlerView(
                        activity,
                        mPaymentHandlerWebContents,
                        mToolbarCoordinator.getView(),
                        thinWebView.getView(),
                        mInputProtector);
        assert mToolbarCoordinator.getToolbarHeightPx() == view.getToolbarHeightPx();
        PropertyModelChangeProcessor changeProcessor =
                PropertyModelChangeProcessor.create(model, view, PaymentHandlerViewBinder::bind);
        mHider =
                () -> {
                    changeProcessor.destroy();
                    bottomSheetController.removeObserver(mediator);
                    bottomSheetController.hideContent(/* content= */ view, /* animate= */ true);
                    uiObserver.onPaymentHandlerUiClosed();
                    assert activity.getWindow() != null;
                    assert activity.getWindow().getDecorView() != null;
                    activity.getWindow().getDecorView().removeOnLayoutChangeListener(mediator);
                    mediator.destroy();
                    thinWebView.destroy();
                    mPaymentHandlerWebContents.destroy();
                };
        boolean isShowSuccess = bottomSheetController.requestShowContent(view, /* animate= */ true);
        if (!isShowSuccess) return null;

        return mPaymentHandlerWebContents;
    }

    private void initializeWebContents(
            WindowAndroid windowAndroid, ContentView webContentView, GURL url) {
        mPaymentHandlerWebContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(webContentView),
                webContentView,
                windowAndroid,
                WebContents.createDefaultInternalsHolder());

        SelectionPopupController controller =
                SelectionPopupController.fromWebContents(mPaymentHandlerWebContents);
        controller.setActionModeCallback(
                new PaymentHandlerActionModeCallback(mPaymentHandlerWebContents));
        controller.setSelectionClient(
                SelectionClient.createSmartSelectionClient(mPaymentHandlerWebContents));

        mPaymentHandlerWebContents
                .getNavigationController()
                .loadUrl(new LoadUrlParams(url.getSpec()));
    }

    /**
     * Get the WebContents of the Payment Handler for testing purpose. In other situations,
     * WebContents should not be leaked outside the Payment Handler.
     *
     * @return The WebContents of the Payment Handler.
     */
    public WebContents getWebContentsForTest() {
        return mPaymentHandlerWebContents;
    }

    /** Hides the payment-handler UI. */
    public void hide() {
        if (mHider == null) return;
        mHider.run();
        mHider = null;
    }

    public void clickSecurityIconForTest() {
        mToolbarCoordinator.clickSecurityIconForTest();
    }

    public void clickCloseButtonForTest() {
        mToolbarCoordinator.clickCloseButtonForTest();
    }

    public void setInputProtectorForTest(InputProtector inputProtector) {
        mInputProtector = inputProtector;
    }
}
