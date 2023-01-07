// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * PaymentHandler view binder, which is stateless. It is called to bind a given model to a given
 * view. Should contain as little business logic as possible.
 */
/* package */ class PaymentHandlerViewBinder {
    /* package */ static void bind(
            PropertyModel model, PaymentHandlerView view, PropertyKey propertyKey) {
        if (PaymentHandlerProperties.CONTENT_VISIBLE_HEIGHT_PX == propertyKey) {
            view.onContentVisibleHeightChanged(
                    model.get(PaymentHandlerProperties.CONTENT_VISIBLE_HEIGHT_PX));
        } else if (PaymentHandlerProperties.BACK_PRESS_CALLBACK == propertyKey) {
            view.setBackPressCallback(model.get(PaymentHandlerProperties.BACK_PRESS_CALLBACK));
        }
    }
}
