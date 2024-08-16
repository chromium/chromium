// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottom_sheet;

import android.content.Context;

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

    public void showSheet() {
        mView = new SimpleNoticeSheetView(mContext);
        PropertyModelChangeProcessor.create(
                new PropertyModel.Builder(SimpleNoticeSheetProperties.ALL_KEYS).build(),
                mView,
                SimpleNoticeSheetViewBinder::bindSimpleNoticeSheetView);

        mSheetController.addObserver(mBottomSheetObserver);
        mSheetController.requestShowContent(mView, true);
    }

    void onDismissed(@StateChangeReason int reason) {
        mSheetController.removeObserver(mBottomSheetObserver);
    }
}
