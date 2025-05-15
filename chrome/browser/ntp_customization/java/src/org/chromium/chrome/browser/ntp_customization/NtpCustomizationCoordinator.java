// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.build.NullUtil.assumeNonNull;
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
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.feed.FeedSettingsCoordinator;
import org.chromium.chrome.browser.ntp_customization.ntp_cards.NtpCardsCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Coordinator of the NTP customization main bottom sheet. */
@NullMarked
public class NtpCustomizationCoordinator {
    /**
     * mDelegate will be passed to every bottom sheet coordinator created by {@link
     * NtpCustomizationCoordinator}.
     */
    private final BottomSheetDelegate mDelegate;

    private final Context mContext;
    private final Supplier<Profile> mProfileSupplier;
    private final int mBottomSheetType;
    private NtpCustomizationMediator mMediator;
    private @MonotonicNonNull NtpCardsCoordinator mNtpCardsCoordinator;
    private @Nullable FeedSettingsCoordinator mFeedSettingsCoordinator;
    private ViewFlipper mViewFlipperView;

    @IntDef({BottomSheetType.MAIN, BottomSheetType.NTP_CARDS, BottomSheetType.FEED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface BottomSheetType {
        int MAIN = 0;
        int NTP_CARDS = 1;
        int FEED = 2;
        int NUM_ENTRIES = 3;
    }

    @IntDef({EntryPointType.MAIN_MENU, EntryPointType.TOOL_BAR, EntryPointType.NEW_TAB_PAGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface EntryPointType {
        int MAIN_MENU = 0;
        int TOOL_BAR = 1;
        int NEW_TAB_PAGE = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * @param context The Context used for displaying the bottom sheet.
     * @param bottomSheetController A controller for managing the bottom sheet's lifecycle and
     *     behavior.
     * @param profileSupplier A supplier for the profile, used to fetch the state of the feeds
     *     section.
     * @param bottomSheetType The bottom sheet type to display independently. If set to `MAIN`, the
     *     main bottom sheet will be shown instead, enabling its full navigation flow, otherwise the
     *     bottom sheet of the bottomSheetType will show by itself.
     */
    public NtpCustomizationCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            Supplier<Profile> profileSupplier,
            @BottomSheetType int bottomSheetType) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        mBottomSheetType = bottomSheetType;
        View contentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.ntp_customization_bottom_sheet, /* root= */ null);
        mViewFlipperView = contentView.findViewById(R.id.ntp_customization_view_flipper);
        contentView.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_ASSERTIVE);

        // This empty OnClickListener is added to the ViewFlipper to prevent TalkBack from
        // unexpectedly triggering the click listeners of its child list items.
        mViewFlipperView.setOnClickListener(v -> {});

        NtpCustomizationBottomSheetContent bottomSheetContent = initBottomSheetContent(contentView);

        // The containerPropertyModel is responsible for managing a BottomSheetDelegate which
        // provides list content and event handlers to a list container view in the bottom sheet.
        PropertyModel containerPropertyModel = null;
        // Skips creating property model for the main bottom sheet if only one bottom sheet
        // should show.
        if (mBottomSheetType == MAIN) {
            View mainBottomSheetView = mViewFlipperView.findViewById(R.id.main_bottom_sheet);
            containerPropertyModel = new PropertyModel(LIST_CONTAINER_KEYS);
            PropertyModelChangeProcessor.create(
                    containerPropertyModel,
                    mainBottomSheetView.findViewById(R.id.ntp_customization_options_container),
                    BottomSheetListContainerViewBinder::bind);
        }

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

        // Skips creating main bottom sheet content if only one bottom sheet should show.
        if (mBottomSheetType == MAIN) {
            // The click listener for each list item in the main bottom sheet should be registered
            // before calling renderListContent().
            mMediator.registerClickListener(NTP_CARDS, getOptionClickListener(NTP_CARDS));
            mMediator.registerClickListener(FEED, getOptionClickListener(FEED));
            mMediator.renderListContent();
        }
    }

    @VisibleForTesting
    NtpCustomizationBottomSheetContent initBottomSheetContent(View contentView) {
        return new NtpCustomizationBottomSheetContent(
                contentView,
                mBottomSheetType == MAIN
                        ? () -> mMediator.backPressOnCurrentBottomSheet()
                        : () -> mMediator.dismissBottomSheet(),
                this::destroy,
                () -> mMediator.getCurrentBottomSheetType());
    }

    /**
     * Opens the NTP customization bottom sheet. Depending on the value of `mStandAlonePage`, the
     * function will either show a specific bottom sheet independently or show the main bottom sheet
     * and enable a full navigation flow that begins from the main bottom sheet.
     */
    public void showBottomSheet() {
        switch (mBottomSheetType) {
            case MAIN -> mMediator.showBottomSheet(MAIN);
            case NTP_CARDS -> showNtpCardsBottomSheet();
            case FEED -> showFeedBottomSheet();
            default -> {
                assert false : "Bottom sheet type not supported!";
            }
        }
    }

    private void showNtpCardsBottomSheet() {
        if (mNtpCardsCoordinator == null) {
            mNtpCardsCoordinator = new NtpCardsCoordinator(mContext, mDelegate);
        }
        mMediator.showBottomSheet(NTP_CARDS);
    }

    private void showFeedBottomSheet() {
        if (mFeedSettingsCoordinator == null) {
            mFeedSettingsCoordinator =
                    new FeedSettingsCoordinator(
                            mContext, mDelegate, mProfileSupplier.get().getOriginalProfile());
        }
        mMediator.showBottomSheet(FEED);
    }

    /**
     * Returns a click listener to handle user clicks on the options in the NTP customization main
     * bottom sheet.
     */
    @VisibleForTesting
    View.OnClickListener getOptionClickListener(@BottomSheetType int type) {
        switch (type) {
            case NTP_CARDS -> {
                return v -> showNtpCardsBottomSheet();
            }
            case FEED -> {
                return v -> showFeedBottomSheet();
            }
            default -> {
                assert false : "Bottom sheet type not supported!";
                return assumeNonNull(null);
            }
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

            @Override
            public boolean shouldShowAlone() {
                return mBottomSheetType != MAIN;
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

    @Nullable NtpCardsCoordinator getNtpCardsCoordinatorForTesting() {
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
