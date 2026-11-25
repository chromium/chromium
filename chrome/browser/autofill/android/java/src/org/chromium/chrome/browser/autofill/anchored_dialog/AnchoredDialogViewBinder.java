// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.anchored_dialog;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class AnchoredDialogViewBinder {
    public static void bind(PropertyModel model, AnchoredDialogView view, PropertyKey propertyKey) {
        if (AnchoredDialogProperties.CONTENT == propertyKey) {
            view.setContent(model.get(AnchoredDialogProperties.CONTENT));
        } else if (AnchoredDialogProperties.CONTAINER_VIEW == propertyKey) {
            view.setContainerView(model.get(AnchoredDialogProperties.CONTAINER_VIEW));
        } else if (AnchoredDialogProperties.ON_DISMISS_LISTENER == propertyKey) {
            view.setOnDismissListener(model.get(AnchoredDialogProperties.ON_DISMISS_LISTENER));
        } else if (AnchoredDialogProperties.OFFSET_Y == propertyKey) {
            view.setOffsetY(model.get(AnchoredDialogProperties.OFFSET_Y));
        } else if (AnchoredDialogProperties.VISIBLE == propertyKey) {
            if (model.get(AnchoredDialogProperties.VISIBLE)) {
                view.show();
            } else {
                view.hide();
            }
        }
    }

    // Do not instantiate this class.
    private AnchoredDialogViewBinder() {}
}
