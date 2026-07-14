// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Implements the TouchToFillAutofillComponent. It uses a bottom sheet to prompt the user with the
 * Personal Context Notice.
 */
@NullMarked
public class TouchToFillAutofillCoordinator implements TouchToFillAutofillComponent {
    private final TouchToFillAutofillMediator mMediator = new TouchToFillAutofillMediator();
    private @Nullable
            PropertyModelChangeProcessor<PropertyModel, TouchToFillAutofillView, PropertyKey>
            mModelChangeProcessor;
    private @Nullable TouchToFillAutofillView mView;

    @Override
    public void initialize(
            Context context,
            BottomSheetController sheetController,
            Delegate delegate,
            BottomSheetFocusHelper bottomSheetFocusHelper) {
        PropertyModel model =
                new PropertyModel.Builder(TouchToFillAutofillProperties.ALL_KEYS)
                        .with(TouchToFillAutofillProperties.DISMISS_HANDLER, mMediator::onDismissed)
                        .with(
                                TouchToFillAutofillProperties.ACKNOWLEDGE_HANDLER,
                                mMediator::onNoticeAcknowledged)
                        .with(
                                TouchToFillAutofillProperties.SETTINGS_LINK_HANDLER,
                                mMediator::onSettingsLinkClicked)
                        .build();
        mMediator.initialize(delegate, model, bottomSheetFocusHelper);
        mView = new TouchToFillAutofillView(context, sheetController);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        model, mView, TouchToFillAutofillViewBinder::bind);
    }

    @Override
    public void show() {
        mMediator.show();
    }

    @Override
    public void hide() {
        mMediator.hide();
    }

    @Override
    public void destroy() {
        hide();
        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
            mModelChangeProcessor = null;
        }
        if (mView != null) {
            mView.destroy();
            mView = null;
        }
        mMediator.destroy();
    }
}
