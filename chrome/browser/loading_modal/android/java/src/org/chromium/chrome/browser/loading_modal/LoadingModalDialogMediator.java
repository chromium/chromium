// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.loading_modal;

import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class responsible for initializing the model state and handling button clicks. */
class LoadingModalDialogMediator implements ModalDialogProperties.Controller {
    public LoadingModalDialogMediator() {}

    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {}

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {}
}
