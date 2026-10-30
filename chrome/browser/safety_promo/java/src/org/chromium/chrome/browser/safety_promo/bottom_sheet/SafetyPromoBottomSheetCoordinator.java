// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo.bottom_sheet;

import static org.chromium.chrome.browser.safety_promo.bottom_sheet.SafetyPromoBottomSheetProperties.ON_GOT_IT_CLICKED;
import static org.chromium.chrome.browser.safety_promo.bottom_sheet.SafetyPromoBottomSheetProperties.TITLE_RES_ID;

import android.content.Context;
import android.view.LayoutInflater;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.safety_promo.R;
import org.chromium.chrome.browser.safety_promo.SafetyPromoItem;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the Safety Promo "Show me how" bottom sheet. A new coordinator is created each
 * time the sheet is opened.
 */
@NullMarked
public class SafetyPromoBottomSheetCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final Runnable mOnDestroy;
    private final PropertyModel mModel;
    private final PropertyModelChangeProcessor<
                    PropertyModel, SafetyPromoBottomSheetView, PropertyKey>
            mModelChangeProcessor;
    private final SafetyPromoBottomSheetContent mBottomSheetContent;

    /**
     * @param context The {@link Context} used for inflating the sheet.
     * @param bottomSheetController The {@link BottomSheetController} that shows the sheet.
     * @param item The {@link SafetyPromoItem} whose instructions the sheet shows.
     * @param onDestroy Called after the sheet is closed for any reason and its content destroyed.
     */
    public SafetyPromoBottomSheetCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            SafetyPromoItem item,
            Runnable onDestroy) {
        mBottomSheetController = bottomSheetController;
        mOnDestroy = onDestroy;

        SafetyPromoBottomSheetView view =
                (SafetyPromoBottomSheetView)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.safety_promo_show_me_how_bottom_sheet,
                                        /* root= */ null);
        mModel =
                new PropertyModel.Builder(SafetyPromoBottomSheetProperties.ALL_KEYS)
                        .with(TITLE_RES_ID, item.bottomSheetTitleResId)
                        .with(ON_GOT_IT_CLICKED, _ -> dismissBottomSheet(/* animate= */ true))
                        .build();
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, view, SafetyPromoBottomSheetViewBinder::bind);

        mBottomSheetContent = new SafetyPromoBottomSheetContent(view, this::destroy);
    }

    /** Opens the "Show me how" bottom sheet. */
    public void showBottomSheet() {
        mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
    }

    /** Closes the "Show me how" bottom sheet. */
    public void dismissBottomSheet(boolean animate) {
        mBottomSheetController.hideContent(mBottomSheetContent, animate);
    }

    /** Called when the controller destroys the sheet content. */
    private void destroy() {
        mModel.set(ON_GOT_IT_CLICKED, null);
        mModelChangeProcessor.destroy();
        mOnDestroy.run();
    }
}
