// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.share_tab;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

class QrCodeShareViewBinder implements ViewBinder<PropertyModel, QrCodeShareView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, QrCodeShareView view, PropertyKey propertyKey) {
        if (QrCodeShareViewProperties.QRCODE_BITMAP == propertyKey) {
            view.updateQrCodeBitmap(model.get(QrCodeShareViewProperties.QRCODE_BITMAP));
        } else if (QrCodeShareViewProperties.ERROR_STRING == propertyKey) {
            view.displayErrorMessage(model.get(QrCodeShareViewProperties.ERROR_STRING));
        } else if (QrCodeShareViewProperties.HAS_STORAGE_PERMISSION == propertyKey) {
            view.storagePermissionsChanged(
                    model.get(QrCodeShareViewProperties.HAS_STORAGE_PERMISSION));
        } else if (QrCodeShareViewProperties.CAN_PROMPT_FOR_PERMISSION == propertyKey) {
            view.canPromptForPermissionChanged(
                    model.get(QrCodeShareViewProperties.CAN_PROMPT_FOR_PERMISSION));
        } else if (QrCodeShareViewProperties.IS_ON_FOREGROUND == propertyKey) {
            view.onForegroundChanged(model.get(QrCodeShareViewProperties.IS_ON_FOREGROUND));
        }
    }
}
