// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The view binder for the Screenshot Share Sheet. */
class ScreenshotShareSheetViewBinder {
    public static void bind(
            PropertyModel model, ScreenshotShareSheetView view, PropertyKey propertyKey) {
        if (ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER == propertyKey) {
            view.setNoArgOperationsListeners(
                    model.get(ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER));
        } else if (ScreenshotShareSheetViewProperties.SCREENSHOT_BITMAP == propertyKey) {
            view.updateScreenshotBitmap(
                    model.get(ScreenshotShareSheetViewProperties.SCREENSHOT_BITMAP));
        }
    }
}
