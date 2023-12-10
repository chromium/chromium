// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Data properties for the Long Screenshots area selection dialog. */
final class LongScreenshotsAreaSelectionDialogProperties {
    // Callback handling clicks on the done (check) button.
    public static final WritableObjectPropertyKey<OnClickListener> DONE_BUTTON_CALLBACK =
            new WritableObjectPropertyKey<>();

    // Callback handling clicks on the close (x) button.
    public static final WritableObjectPropertyKey<OnClickListener> CLOSE_BUTTON_CALLBACK =
            new WritableObjectPropertyKey<>();

    // Callback handling clicks on the down arrow button.
    public static final WritableObjectPropertyKey<OnClickListener> DOWN_BUTTON_CALLBACK =
            new WritableObjectPropertyKey<>();

    // Callback handling clicks on the up arrow button.
    public static final WritableObjectPropertyKey<OnClickListener> UP_BUTTON_CALLBACK =
            new WritableObjectPropertyKey<>();

    private LongScreenshotsAreaSelectionDialogProperties() {}

    static PropertyModel.Builder defaultModelBuilder() {
        return new PropertyModel.Builder(
                DONE_BUTTON_CALLBACK,
                CLOSE_BUTTON_CALLBACK,
                DOWN_BUTTON_CALLBACK,
                UP_BUTTON_CALLBACK);
    }
}
