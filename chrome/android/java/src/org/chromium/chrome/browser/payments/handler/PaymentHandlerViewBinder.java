// Copyright 2019 The Chromium Authors. All rights reserved.
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
        if (PaymentHandlerProperties.BOTTOM_SHEET_HEIGHT_FRACTION == propertyKey) {
            view.onHeightFractionChanged(
                    model.get(PaymentHandlerProperties.BOTTOM_SHEET_HEIGHT_FRACTION));
        }
    }
}
