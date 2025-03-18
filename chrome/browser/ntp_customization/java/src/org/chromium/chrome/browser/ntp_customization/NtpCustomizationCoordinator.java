// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.NTP_CARDS_BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.NTP_CARDS_OPTION_CLICK_LISTENER;

import android.content.Context;
import android.support.annotation.IntDef;
import android.support.annotation.VisibleForTesting;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ViewFlipper;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Coordinator of the ntp customization main bottom sheet. */
public class NtpCustomizationCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final PropertyModel mPropertyModel;
    private NtpCustomizationMediator mMediator;
    private NtpCardsCoordinator mNtpCardsCoordinator;
    private View mContentView;
    private ViewFlipper mViewFlipperView;

    /**
     * mDelegate will be passed to every bottom sheet coordinator created by {@link
     * NtpCustomizationCoordinator}.
     */
    private BottomSheetDelegate mDelegate;

    @IntDef({BottomSheetType.MAIN, BottomSheetType.NTP_CARDS})
    @Retention(RetentionPolicy.SOURCE)
    @interface BottomSheetType {
        int MAIN = 0;
        int NTP_CARDS = 1;
    }

    public NtpCustomizationCoordinator(
            Context context, BottomSheetController bottomSheetController) {
        this(
                context,
                bottomSheetController,
                new PropertyModel(NtpCustomizationViewProperties.ALL_KEYS));
    }

    @VisibleForTesting
    NtpCustomizationCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            PropertyModel propertyModel) {
        mBottomSheetController = bottomSheetController;
        mPropertyModel = propertyModel;
        mContentView =
                LayoutInflater.from(context)
                        .inflate(R.layout.ntp_customization_bottom_sheet, /* root= */ null);
        mViewFlipperView = mContentView.findViewById(R.id.ntp_customization_view_flipper);
        PropertyModelChangeProcessor.create(
                mPropertyModel, mViewFlipperView, NtpCustomizationViewBinder::bind);

        NtpCustomizationBottomSheetContent bottomSheetContent =
                new NtpCustomizationBottomSheetContent(
                        mContentView,
                        /* backPressRunnable= */ () -> mMediator.backPressOnCurrentBottomSheet(),
                        this::destroy);

        mMediator =
                new NtpCustomizationMediator(
                        mBottomSheetController, bottomSheetContent, mPropertyModel);

        LayoutInflater.from(context)
                .inflate(R.layout.ntp_customization_main_bottom_sheet, mViewFlipperView, true);
        mMediator.registerBottomSheetLayout(MAIN);

        mDelegate =
                new BottomSheetDelegate() {
                    @Override
                    public void registerBottomSheetLayout(int type, View view) {
                        mViewFlipperView.addView(view);
                        mMediator.registerBottomSheetLayout(type);
                    }

                    @Override
                    public void backPressOnCurrentBottomSheet() {
                        mMediator.backPressOnCurrentBottomSheet();
                    }
                };

        mPropertyModel.set(
                NTP_CARDS_OPTION_CLICK_LISTENER,
                v -> {
                    if (mNtpCardsCoordinator == null) {
                        mNtpCardsCoordinator =
                                new NtpCardsCoordinator(context, mDelegate, mPropertyModel);
                    }
                    mMediator.showBottomSheet(BottomSheetType.NTP_CARDS);
                });
    }

    /** Opens the new tab page customization main bottom sheet. */
    public void showBottomSheet() {
        mMediator.showBottomSheet(MAIN);
    }

    /** Removes all click listeners, all views inside the view flipper and destroy the mediator. */
    public void destroy() {
        mPropertyModel.set(NTP_CARDS_OPTION_CLICK_LISTENER, null);
        if (mNtpCardsCoordinator != null) {
            mPropertyModel.set(NTP_CARDS_BACK_PRESS_HANDLER, null);
        }
        mViewFlipperView.removeAllViews();
        mMediator.destroy();
    }

    void setMediatorForTesting(NtpCustomizationMediator mediator) {
        mMediator = mediator;
    }

    BottomSheetDelegate getDelegateForTesting() {
        return mDelegate;
    }

    NtpCardsCoordinator getNtpCardsCoordinatorForTesting() {
        return mNtpCardsCoordinator;
    }

    View getContentViewForTesting() {
        return mContentView;
    }

    void setViewFlipperForTesting(ViewFlipper viewFlipper) {
        mViewFlipperView = viewFlipper;
    }
}
