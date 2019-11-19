// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.payments.PaymentsExperimentalFeatures;
import org.chromium.chrome.browser.payments.handler.toolbar.PaymentHandlerToolbarCoordinator;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.net.URI;

/**
 * PaymentHandler coordinator, which owns the component overall, i.e., creates other objects in the
 * component and connects them. It decouples the implementation of this component from other
 * components and acts as the point of contact between them. Any code in this component that needs
 * to interact with another component does that through this coordinator.
 */
public class PaymentHandlerCoordinator {
    private Runnable mHider;

    /** Constructs the payment-handler component coordinator. */
    public PaymentHandlerCoordinator() {
        assert isEnabled();
    }

    /**
     * Shows the payment-handler UI.
     *
     * @param chromeActivity The activity where the UI should be shown.
     * @param url The url of the payment handler app, i.e., that of
     *         "PaymentRequestEvent.openWindow(url)".
     * @param isIncognito Whether the tab is in incognito mode.
     * @return Whether the payment-handler UI was shown. Can be false if the UI was suppressed.
     */
    public boolean show(ChromeActivity activity, URI url, boolean isIncognito) {
        assert mHider == null : "Already showing payment-handler UI";

        WebContents webContents =
                WebContentsFactory.createWebContents(isIncognito, /*initiallyHidden=*/false);
        ContentView webContentView = ContentView.createContentView(activity, webContents);
        webContents.initialize(ChromeVersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(webContentView), webContentView,
                activity.getWindowAndroid(), WebContents.createDefaultInternalsHolder());
        webContents.getNavigationController().loadUrl(new LoadUrlParams(url.toString()));

        PropertyModel model = new PropertyModel.Builder(PaymentHandlerProperties.ALL_KEYS).build();
        PaymentHandlerMediator mediator =
                new PaymentHandlerMediator(model, this::hide, webContents);
        BottomSheetController bottomSheetController = activity.getBottomSheetController();
        bottomSheetController.addObserver(mediator);
        webContents.addObserver(mediator);

        PaymentHandlerToolbarCoordinator toolbarCoordinator = new PaymentHandlerToolbarCoordinator(
                activity, webContents, () -> { mHider.run(); });
        PaymentHandlerView view = new PaymentHandlerView(
                activity, webContents, webContentView, toolbarCoordinator.getView());
        assert toolbarCoordinator.getToolbarHeightPx() == view.getToolbarHeightPx();
        PropertyModelChangeProcessor changeProcessor =
                PropertyModelChangeProcessor.create(model, view, PaymentHandlerViewBinder::bind);
        mHider = () -> {
            changeProcessor.destroy();
            bottomSheetController.removeObserver(mediator);
            bottomSheetController.hideContent(/*content=*/view, /*animate=*/true);
            webContents.destroy();
        };
        boolean result = bottomSheetController.requestShowContent(view, /*animate=*/true);
        if (result) bottomSheetController.expandSheet();
        return result;
    }

    /** Hides the payment-handler UI. */
    public void hide() {
        if (mHider == null) return;
        mHider.run();
    }

    /**
     * @return Whether this solution (as opposed to the Chrome-custom-tab based solution) of
     *     PaymentHandler is enabled. This solution is intended to replace the other
     *     solution.
     */
    public static boolean isEnabled() {
        // Enabling the flag of either ScrollToExpand or PaymentsExperimentalFeatures will enable
        // this feature.
        return PaymentsExperimentalFeatures.isEnabled(
                ChromeFeatureList.SCROLL_TO_EXPAND_PAYMENT_HANDLER);
    }
}
