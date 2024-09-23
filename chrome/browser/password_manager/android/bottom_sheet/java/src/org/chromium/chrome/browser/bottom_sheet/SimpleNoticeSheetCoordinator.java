// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottom_sheet;

import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.BUTTON_ACTION;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.BUTTON_TITLE;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TEXT;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TITLE;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator of the simple notice sheet. */
public class SimpleNoticeSheetCoordinator {
    private Context mContext;
    private BottomSheetController mSheetController;
    private SimpleNoticeSheetView mView;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                    super.onSheetClosed(reason);
                    if (mSheetController.getCurrentSheetContent() != null
                            && mSheetController.getCurrentSheetContent() == mView) {
                        onDismissed(reason);
                    }
                }
            };

    public SimpleNoticeSheetCoordinator(Context context, BottomSheetController sheetController) {
        mContext = context;
        mSheetController = sheetController;
    }

    /**
     * Shows the simple notice sheet with the properties defined in the model. The sheet won't show
     * if the model is incomplete.
     *
     * @param model determines the looks and functionality of the sheet.
     */
    public void showSheet(PropertyModel model) {
        checkModelValidity(model);
        mView = new SimpleNoticeSheetView(mContext);
        PropertyModelChangeProcessor.create(
                model, mView, SimpleNoticeSheetViewBinder::bindSimpleNoticeSheetView);

        mSheetController.addObserver(mBottomSheetObserver);
        mSheetController.requestShowContent(mView, true);
    }

    void onDismissed(@StateChangeReason int reason) {
        mSheetController.removeObserver(mBottomSheetObserver);
    }

    void checkModelValidity(PropertyModel model) {
        assert !TextUtils.isEmpty(model.get(SHEET_TITLE));
        assert !TextUtils.isEmpty(model.get(SHEET_TEXT));
        assert !TextUtils.isEmpty(model.get(BUTTON_TITLE));
        assert model.get(BUTTON_ACTION) != null;
    }
}
