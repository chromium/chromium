// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.scan_tab;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

class QrCodeScanViewProperties {
    public static final WritableBooleanPropertyKey HAS_CAMERA_PERMISSION =
            new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey CAN_PROMPT_FOR_PERMISSION =
            new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey IS_ON_FOREGROUND =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
            HAS_CAMERA_PERMISSION, CAN_PROMPT_FOR_PERMISSION, IS_ON_FOREGROUND};
}
