// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler.toolbar;

import android.view.View;

import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * PaymentHandlerToolbar view binder, which is stateless. It is called to bind a given model to a
 * given view. Should contain as little business logic as possible.
 */
/* package */ class PaymentHandlerToolbarViewBinder {
    /* package */ static void bind(
            PropertyModel model, PaymentHandlerToolbarView view, PropertyKey propertyKey) {
        if (PaymentHandlerToolbarProperties.URL == propertyKey) {
            String origin =
                    UrlFormatter.formatUrlForSecurityDisplay(
                            model.get(PaymentHandlerToolbarProperties.URL),
                            SchemeDisplay.OMIT_HTTP_AND_HTTPS);
            view.mOriginView.setText(origin);
        } else if (PaymentHandlerToolbarProperties.TITLE == propertyKey) {
            view.mTitleView.setText(model.get(PaymentHandlerToolbarProperties.TITLE));
        } else if (PaymentHandlerToolbarProperties.LOAD_PROGRESS == propertyKey) {
            view.mProgressBar.setProgress(
                    Math.round(model.get(PaymentHandlerToolbarProperties.LOAD_PROGRESS) * 100));
        } else if (PaymentHandlerToolbarProperties.PROGRESS_VISIBLE == propertyKey) {
            boolean visible = model.get(PaymentHandlerToolbarProperties.PROGRESS_VISIBLE);
            view.mProgressBar.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
        } else if (PaymentHandlerToolbarProperties.SECURITY_ICON == propertyKey) {
            int iconRes = model.get(PaymentHandlerToolbarProperties.SECURITY_ICON);
            view.mSecurityIconView.setImageResource(iconRes);
        } else if (PaymentHandlerToolbarProperties.SECURITY_ICON_CONTENT_DESCRIPTION
                == propertyKey) {
            String description =
                    model.get(PaymentHandlerToolbarProperties.SECURITY_ICON_CONTENT_DESCRIPTION);
            view.mSecurityIconView.setContentDescription(description);
        } else if (PaymentHandlerToolbarProperties.SECURITY_ICON_ON_CLICK_CALLBACK == propertyKey) {
            view.mSecurityIconView.setOnClickListener(
                    (v) -> {
                        model.get(PaymentHandlerToolbarProperties.SECURITY_ICON_ON_CLICK_CALLBACK)
                                .run();
                    });
        } else if (PaymentHandlerToolbarProperties.CLOSE_BUTTON_ON_CLICK_CALLBACK == propertyKey) {
            view.mCloseButton.setOnClickListener(
                    (v) -> {
                        model.get(PaymentHandlerToolbarProperties.CLOSE_BUTTON_ON_CLICK_CALLBACK)
                                .run();
                    });
        }
    }
}
