// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.ItemType.FILL_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.ItemType.HEADER;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.ItemType.TEXT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.autofill.TouchToFillAutofillProperties.SHEET_ITEMS;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonViewBinder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Implements the TouchToFillAutofillComponent. It uses a bottom sheet to prompt the user with the
 * Personal Context Notice.
 */
@NullMarked
public class TouchToFillAutofillCoordinator implements TouchToFillAutofillComponent {
    private final TouchToFillAutofillMediator mMediator;
    private final PropertyModelChangeProcessor<PropertyModel, TouchToFillAutofillView, PropertyKey>
            mModelChangeProcessor;
    private final TouchToFillAutofillView mView;

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

        setUpSheetItems(mMediator.getModel(), mView);

        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(), mView, TouchToFillAutofillViewBinder::bind);
    }

    static void setUpSheetItems(PropertyModel model, TouchToFillAutofillView view) {
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(model.get(SHEET_ITEMS));
        adapter.registerType(
                HEADER,
                TouchToFillCommonViewBinder::createHeaderItemView,
                TouchToFillCommonViewBinder::bindHeaderView);
        adapter.registerType(
                FILL_BUTTON,
                TouchToFillCommonViewBinder::createFillButtonView,
                TouchToFillCommonViewBinder::bindButtonView);
        adapter.registerType(
                TEXT_BUTTON,
                TouchToFillCommonViewBinder::createTextButtonView,
                TouchToFillCommonViewBinder::bindButtonView);

        view.setSheetItemListAdapter(adapter);
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
        mModelChangeProcessor.destroy();
        mView.destroy();
    }

    PropertyModel getModelForTesting() {
        return mMediator.getModel();
    }
}
