// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LAYOUT_TO_DISPLAY;

import android.widget.ViewFlipper;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashMap;
import java.util.Map;

/**
 * A mediator class that manages the view flipper and {@link BottomSheetContent} of ntp
 * customization bottom sheets.
 */
public class NtpCustomizationMediator {
    /**
     * A map of <{@link NtpCustomizationCoordinator.BottomSheetType}, view's index in the {@link
     * ViewFlipper}>.
     */
    private final Map<Integer, Integer> mViewFlipperMap;

    private final BottomSheetController mBottomSheetController;
    private final BottomSheetContent mBottomSheetContent;
    private final PropertyModel mPropertyModel;
    private Integer mCurrentBottomSheet;

    public NtpCustomizationMediator(
            BottomSheetController bottomSheetController,
            BottomSheetContent bottomSheetContent,
            PropertyModel propertyModel) {
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mPropertyModel = propertyModel;
        mViewFlipperMap = new HashMap<>();
    }

    /**
     * Records the position of the bottom sheet layout in the view flipper view.
     *
     * @param type The type of the bottom sheet.
     */
    void registerBottomSheetLayout(@NtpCustomizationCoordinator.BottomSheetType int type) {
        if (mViewFlipperMap.containsKey(type)) return;

        mViewFlipperMap.put(type, mViewFlipperMap.size());
    }

    /** Shows the given type of the bottom sheet. */
    void showBottomSheet(@NtpCustomizationCoordinator.BottomSheetType int type) {
        assert mViewFlipperMap.get(type) != null;

        int viewIndex = mViewFlipperMap.get(type);
        mPropertyModel.set(LAYOUT_TO_DISPLAY, viewIndex);
        boolean shouldRequestShowContent = mCurrentBottomSheet == null;
        mCurrentBottomSheet = type;

        // requestShowContent() is called only when showBottomSheet() is called at the first time.
        if (shouldRequestShowContent) {
            mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
        }
    }

    /** Handles system back press and back button clicks on the bottom sheet. */
    void backPressOnCurrentBottomSheet() {
        if (mCurrentBottomSheet == null) return;

        if (mCurrentBottomSheet == MAIN) {
            mBottomSheetController.hideContent(mBottomSheetContent, true);
            mCurrentBottomSheet = null;
        } else {
            showBottomSheet(MAIN);
        }
    }

    /** Clears the map */
    void destroy() {
        mViewFlipperMap.clear();
    }

    Map<Integer, Integer> getViewFlipperMapForTesting() {
        return mViewFlipperMap;
    }

    @NtpCustomizationCoordinator.BottomSheetType
    Integer getCurrentBottomSheetForTesting() {
        return mCurrentBottomSheet;
    }

    void setCurrentBottomSheetForTesting(
            @NtpCustomizationCoordinator.BottomSheetType int bottomSheetType) {
        mCurrentBottomSheet = bottomSheetType;
    }
}
