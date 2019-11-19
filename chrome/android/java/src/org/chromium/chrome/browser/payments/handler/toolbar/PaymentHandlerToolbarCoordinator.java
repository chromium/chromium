// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler.toolbar;

import android.content.Context;
import android.view.View;

import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * PaymentHandlerToolbar coordinator, which owns the component overall, i.e., creates other objects
 * in the component and connects them. It decouples the implementation of this component from other
 * components and acts as the point of contact between them. Any code in this component that needs
 * to interact with another component does that through this coordinator.
 */
public class PaymentHandlerToolbarCoordinator {
    private Runnable mHider;
    private PaymentHandlerToolbarView mToolbarView;

    /**
     * Observer for the error of the payment handler toolbar.
     */
    public interface ErrorObserver {
        /**
         * Called when the UI gets an error
         */
        void onError();
    }

    /** Constructs the payment-handler toolbar component coordinator. */
    public PaymentHandlerToolbarCoordinator(
            Context context, WebContents webContents, ErrorObserver errorObserver) {
        mToolbarView = new PaymentHandlerToolbarView(context);
        PropertyModel model = new PropertyModel.Builder(PaymentHandlerToolbarProperties.ALL_KEYS)
                                      .with(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE, true)
                                      .with(PaymentHandlerToolbarProperties.LOAD_PROGRESS, 0)
                                      .with(PaymentHandlerToolbarProperties.SECURITY_ICON,
                                              ConnectionSecurityLevel.NONE)
                                      .build();
        PaymentHandlerToolbarMediator mediator =
                new PaymentHandlerToolbarMediator(model, webContents, errorObserver);
        webContents.addObserver(mediator);
        PropertyModelChangeProcessor changeProcessor = PropertyModelChangeProcessor.create(
                model, mToolbarView, PaymentHandlerToolbarViewBinder::bind);
    }

    /** @return The height of the toolbar in px. */
    public int getToolbarHeightPx() {
        return mToolbarView.getToolbarHeightPx();
    }

    /** @return The toolbar of the PaymentHandler. */
    public View getView() {
        return mToolbarView.getView();
    }
}
