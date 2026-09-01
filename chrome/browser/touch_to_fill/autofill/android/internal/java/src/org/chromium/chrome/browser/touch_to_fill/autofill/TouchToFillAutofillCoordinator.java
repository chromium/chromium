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
    private final TouchToFillAutofillMediator mMediator;
    private @Nullable
            PropertyModelChangeProcessor<PropertyModel, TouchToFillAutofillView, PropertyKey>
            mModelChangeProcessor;
    private @Nullable TouchToFillAutofillView mView;

    /**
     * Constructs a new {@link TouchToFillAutofillCoordinator}.
     *
     * @param context The {@link Context} for accessing string resources and creating the view.
     * @param sheetController The {@link BottomSheetController} used to display and manage the
     *     bottom sheet.
     * @param delegate The {@link Delegate} handling the interaction callbacks from the view.
     * @param bottomSheetFocusHelper The {@link BottomSheetFocusHelper} used to manage and restore
     *     accessibility focus for the bottom sheet.
     */
    public TouchToFillAutofillCoordinator(
            Context context,
            BottomSheetController sheetController,
            Delegate delegate,
            BottomSheetFocusHelper bottomSheetFocusHelper) {
        mMediator = new TouchToFillAutofillMediator(delegate, bottomSheetFocusHelper);
        mView = new TouchToFillAutofillView(context, sheetController);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(), mView, TouchToFillAutofillViewBinder::bind);
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
    }
}
