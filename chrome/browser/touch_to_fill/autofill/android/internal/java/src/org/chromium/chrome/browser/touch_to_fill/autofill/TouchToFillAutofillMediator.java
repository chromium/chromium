// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.VISIBLE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillComponent.Delegate;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the business logic for the TouchToFillAutofill MVC component. It is responsible for
 * binding the user interaction events to the delegate and controlling the model.
 */
@NullMarked
class TouchToFillAutofillMediator {
    private @Nullable Delegate mDelegate;
    private @Nullable PropertyModel mModel;
    private @Nullable BottomSheetFocusHelper mBottomSheetFocusHelper;
    private boolean mWasDismissed;

    void initialize(
            Delegate delegate, PropertyModel model, BottomSheetFocusHelper bottomSheetFocusHelper) {
        mDelegate = delegate;
        mModel = model;
        mBottomSheetFocusHelper = bottomSheetFocusHelper;
    }

    void show() {
        mWasDismissed = false;
        assumeNonNull(mBottomSheetFocusHelper).registerForOneTimeUse();
        assumeNonNull(mModel).set(VISIBLE, true);
    }

    void hide() {
        onDismissed();
    }

    private boolean dismiss() {
        if (mWasDismissed) return false;
        mWasDismissed = true;
        assumeNonNull(mModel).set(VISIBLE, false);
        return true;
    }

    void onNoticeAcknowledged() {
        if (!dismiss()) return;
        assumeNonNull(mDelegate).onNoticeAcknowledged();
    }

    void onSettingsLinkClicked() {
        if (!dismiss()) return;
        assumeNonNull(mDelegate).onSettingsLinkClicked();
    }

    void onDismissed() {
        if (!dismiss()) return;
        assumeNonNull(mDelegate).onDismissed();
    }

    void destroy() {
        mDelegate = null;
        mModel = null;
        mBottomSheetFocusHelper = null;
    }
}
