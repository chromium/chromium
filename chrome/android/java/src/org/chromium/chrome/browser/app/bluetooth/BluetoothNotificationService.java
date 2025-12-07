// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bluetooth;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.base.SplitCompatService;

/** See {@link BluetoothNotificationServiceImpl}. */
@NullMarked
public class BluetoothNotificationService extends SplitCompatService {
    @SuppressWarnings("FieldCanBeFinal") // @IdentifierNameString requires non-final
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.app.bluetooth.BluetoothNotificationServiceImpl";

    public BluetoothNotificationService() {
        super(sImplClassName);
    }
}
