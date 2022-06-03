// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.scan_tab;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

class QrCodeScanViewBinder implements ViewBinder<PropertyModel, QrCodeScanView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, QrCodeScanView view, PropertyKey propertyKey) {
        if (QrCodeScanViewProperties.HAS_CAMERA_PERMISSION == propertyKey) {
            view.cameraPermissionsChanged(
                    model.get(QrCodeScanViewProperties.HAS_CAMERA_PERMISSION));
        } else if (QrCodeScanViewProperties.IS_ON_FOREGROUND == propertyKey) {
            view.onForegroundChanged(model.get(QrCodeScanViewProperties.IS_ON_FOREGROUND));
        } else if (QrCodeScanViewProperties.CAN_PROMPT_FOR_PERMISSION == propertyKey) {
            view.canPromptForPermissionChanged(
                    model.get(QrCodeScanViewProperties.CAN_PROMPT_FOR_PERMISSION));
        }
    }
}
