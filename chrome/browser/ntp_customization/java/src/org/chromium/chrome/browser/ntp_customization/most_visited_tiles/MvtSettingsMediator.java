// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.most_visited_tiles;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the Most Visited Tiles settings bottom sheet. */
@NullMarked
public class MvtSettingsMediator {
    private final PropertyModel mBottomSheetPropertyModel;
    private final BottomSheetDelegate mBottomSheetDelegate;

    public MvtSettingsMediator(
            PropertyModel bottomSheetPropertyModel, BottomSheetDelegate delegate) {
        mBottomSheetPropertyModel = bottomSheetPropertyModel;
        mBottomSheetDelegate = delegate;

        // Hides the back button when the mvt settings bottom sheet is displayed standalone.
        mBottomSheetPropertyModel.set(
                BACK_PRESS_HANDLER,
                delegate.shouldShowAlone()
                        ? null
                        : v -> mBottomSheetDelegate.backPressOnCurrentBottomSheet());
    }

    void destroy() {
        mBottomSheetPropertyModel.set(BACK_PRESS_HANDLER, null);
    }
}
