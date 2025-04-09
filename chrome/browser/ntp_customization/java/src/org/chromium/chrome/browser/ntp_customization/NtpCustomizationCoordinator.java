// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LAYOUT_TO_DISPLAY;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_KEYS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.VIEW_FLIPPER_KEYS;

import android.content.Context;
import android.support.annotation.IntDef;
import android.support.annotation.VisibleForTesting;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ViewFlipper;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ntp_customization.feed.FeedSettingsCoordinator;
import org.chromium.chrome.browser.ntp_customization.ntp_cards.NtpCardsCoordinator;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Coordinator of the NTP customization main bottom sheet. */
public class NtpCustomizationCoordinator {
    /**
     * mDelegate will be passed to every bottom sheet coordinator created by {@link
     * NtpCustomizationCoordinator}.
     */
    private final BottomSheetDelegate mDelegate;

    private final Context mContext;
    private final Supplier<ProfileProvider> mProfileSupplier;
    private NtpCustomizationMediator mMediator;
    private NtpCardsCoordinator mNtpCardsCoordinator;
    private FeedSettingsCoordinator mFeedSettingsCoordinator;
    private ViewFlipper mViewFlipperView;

    @IntDef({BottomSheetType.MAIN, BottomSheetType.NTP_CARDS, BottomSheetType.FEED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface BottomSheetType {
        int MAIN = 0;
        int NTP_CARDS = 1;
        int FEED = 2;
        int NUM_ENTRIES = 3;
    }

    @IntDef({EntryPointType.MAIN_MENU, EntryPointType.TOOL_BAR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface EntryPointType {
        int MAIN_MENU = 0;
        int TOOL_BAR = 1;
        int NUM_ENTRIES = 2;
    }

    public NtpCustomizationCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            Supplier<ProfileProvider> profileSupplier) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        View contentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.ntp_customization_bottom_sheet, /* root= */ null);
        mViewFlipperView = contentView.findViewById(R.id.ntp_customization_view_flipper);

        NtpCustomizationBottomSheetContent bottomSheetContent =
                new NtpCustomizationBottomSheetContent(
                        contentView,
                        /* backPressRunnable= */ () -> mMediator.backPressOnCurrentBottomSheet(),
                        this::destroy);

        // The containerPropertyModel is responsible for managing a BottomSheetDelegate which
        // provides list content and event handlers to a list container view in the bottom sheet.
        View mainBottomSheetView = mViewFlipperView.findViewById(R.id.main_bottom_sheet);
        PropertyModel containerPropertyModel = new PropertyModel(LIST_CONTAINER_KEYS);
        PropertyModelChangeProcessor.create(
                containerPropertyModel,
                mainBottomSheetView.findViewById(R.id.ntp_customization_options_container),
                BottomSheetListContainerViewBinder::bind);

        // The viewFlipperPropertyModel is responsible for controlling which bottom sheet layout to
        // display.
        PropertyModel viewFlipperPropertyModel = new PropertyModel(VIEW_FLIPPER_KEYS);
        PropertyModelChangeProcessor.create(
                viewFlipperPropertyModel,
                mViewFlipperView,
                NtpCustomizationCoordinator::bindViewFlipper);

        mMediator =
                new NtpCustomizationMediator(
                        bottomSheetController,
                        bottomSheetContent,
                        viewFlipperPropertyModel,
                        containerPropertyModel,
                        mProfileSupplier);
        mMediator.registerBottomSheetLayout(MAIN);

        mDelegate = createBottomSheetDelegate();

        // The click listener for each list item in the main bottom sheet should be registered
        // before calling renderListContent().
        mMediator.registerClickListener(NTP_CARDS, getOptionClickListener(NTP_CARDS));
        mMediator.registerClickListener(FEED, getOptionClickListener(FEED));
        mMediator.renderListContent();
    }

    /** Opens the NTP customization main bottom sheet. */
    public void showBottomSheet() {
        mMediator.showBottomSheet(MAIN);
    }

    /**
     * Returns a click listener to handle user clicks on the options in the NTP customization main
     * bottom sheet.
     */
    @VisibleForTesting
    View.OnClickListener getOptionClickListener(@BottomSheetType int type) {
        switch (type) {
            case NTP_CARDS:
                return v -> {
                    if (mNtpCardsCoordinator == null) {
                        mNtpCardsCoordinator = new NtpCardsCoordinator(mContext, mDelegate);
                    }
                    mMediator.showBottomSheet(NTP_CARDS);
                };
            case FEED:
                return v -> {
                    if (mFeedSettingsCoordinator == null) {
                        mFeedSettingsCoordinator =
                                new FeedSettingsCoordinator(
                                        mContext,
                                        mDelegate,
                                        mProfileSupplier.get().getOriginalProfile());
                    }
                    mMediator.showBottomSheet(FEED);
                };
            default:
                assert false : "Bottom sheet type not supported!";
                return null;
        }
    }

    /**
     * Returns a {@link BottomSheetDelegate} which adds layouts to the view flipper and handles
     * clicks on the back button in the bottom sheet.
     */
    BottomSheetDelegate createBottomSheetDelegate() {
        return new BottomSheetDelegate() {
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
    }

    /**
     * Binds the property model to the view flipper view to control which bottom sheet layout to
     * display.
     *
     * @param view The view flipper view.
     */
    static void bindViewFlipper(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == LAYOUT_TO_DISPLAY) {
            ((ViewFlipper) view).setDisplayedChild(model.get(LAYOUT_TO_DISPLAY));
        }
    }

    /**
     * Clears all views inside the view flipper as well as destroys the mediator and coordinators.
     */
    public void destroy() {
        mViewFlipperView.removeAllViews();
        mMediator.destroy();
        if (mNtpCardsCoordinator != null) {
            mNtpCardsCoordinator.destroy();
        }
        if (mFeedSettingsCoordinator != null) {
            mFeedSettingsCoordinator.destroy();
        }
    }

    BottomSheetDelegate getBottomSheetDelegateForTesting() {
        return mDelegate;
    }

    NtpCardsCoordinator getNtpCardsCoordinatorForTesting() {
        return mNtpCardsCoordinator;
    }

    void setViewFlipperForTesting(ViewFlipper viewFlipper) {
        mViewFlipperView = viewFlipper;
    }

    void setNtpCardsCoordinatorForTesting(NtpCardsCoordinator coordinator) {
        mNtpCardsCoordinator = coordinator;
    }

    void setMediatorForTesting(NtpCustomizationMediator mediator) {
        mMediator = mediator;
    }
}
