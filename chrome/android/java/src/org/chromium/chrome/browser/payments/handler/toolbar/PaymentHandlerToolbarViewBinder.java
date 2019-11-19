// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler.toolbar;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * PaymentHandlerToolbar view binder, which is stateless. It is called to bind a given model to a
 * given view. Should contain as little business logic as possible.
 */
/* package */ class PaymentHandlerToolbarViewBinder {
    /* package */ static void bind(
            PropertyModel model, PaymentHandlerToolbarView view, PropertyKey propertyKey) {
        if (PaymentHandlerToolbarProperties.ORIGIN == propertyKey) {
            view.mOriginView.setText(model.get(PaymentHandlerToolbarProperties.ORIGIN).toString());
        } else if (PaymentHandlerToolbarProperties.TITLE == propertyKey) {
            view.mTitleView.setText(model.get(PaymentHandlerToolbarProperties.TITLE));
        } else if (PaymentHandlerToolbarProperties.LOAD_PROGRESS == propertyKey) {
            view.mProgressBar.setProgress(
                    Math.round(model.get(PaymentHandlerToolbarProperties.LOAD_PROGRESS) * 100));
        } else if (PaymentHandlerToolbarProperties.PROGRESS_VISIBLE == propertyKey) {
            boolean visible = model.get(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE);
            view.mProgressBar.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
        } else if (PaymentHandlerToolbarProperties.SECURITY_ICON == propertyKey) {
            int securityIconResource = model.get(PaymentHandlerToolbarProperties.SECURITY_ICON);
            view.mSecurityIconView.setImageResource(securityIconResource);
        }
    }
}
