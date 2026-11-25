// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.anchored_dialog;

import android.view.View;
import android.widget.PopupWindow;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

@NullMarked
class AnchoredDialogProperties {
    static final WritableObjectPropertyKey<BottomSheetContent> CONTENT =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<View> CONTAINER_VIEW = new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<PopupWindow.OnDismissListener> ON_DISMISS_LISTENER =
            new WritableObjectPropertyKey<>();
    static final WritableIntPropertyKey OFFSET_Y = new WritableIntPropertyKey();
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS = {
        CONTENT, CONTAINER_VIEW, ON_DISMISS_LISTENER, OFFSET_Y, VISIBLE,
    };

    // Do not instantiate this class.
    private AnchoredDialogProperties() {}
}
