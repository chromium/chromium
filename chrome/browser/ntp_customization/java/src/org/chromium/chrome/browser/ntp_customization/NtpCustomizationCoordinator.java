// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MVT;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LAYOUT_TO_DISPLAY;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_KEYS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.VIEW_FLIPPER_KEYS;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ViewFlipper;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.feed.FeedSettingsCoordinator;
import org.chromium.chrome.browser.ntp_customization.most_visited_tiles.MvtSettingsCoordinator;
import org.chromium.chrome.browser.ntp_customization.ntp_cards.NtpCardsCoordinator;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Supplier;

/** Coordinator of the NTP customization main bottom sheet. */
@NullMarked
public class NtpCustomizationCoordinator {
    /**
     * mDelegate will be passed to every bottom sheet coordinator created by {@link
     * NtpCustomizationCoordinator}.
     */
    private final BottomSheetDelegate mDelegate;

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final Supplier<@Nullable Profile> mProfileSupplier;
    private final int mBottomSheetType;
    private NtpCustomizationMediator mMediator;
    private @Nullable MvtSettingsCoordinator mMvtSettingCoordinator;
    private @MonotonicNonNull NtpCardsCoordinator mNtpCardsCoordinator;
    private @Nullable FeedSettingsCoordinator mFeedSettingsCoordinator;
    private @Nullable NtpThemeCoordinator mNtpThemeCoordinator;
    private ViewFlipper mViewFlipperView;

    /**
     * New Tab Page Customization bottom sheet type.
     *
     * <p>These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused. See tools/metrics/histograms/enums.xml.
     */
    @IntDef({
        BottomSheetType.MAIN,
        BottomSheetType.NTP_CARDS,
        BottomSheetType.FEED,
        BottomSheetType.THEME,
        BottomSheetType.MVT,
        BottomSheetType.CHROME_COLORS,
        BottomSheetType.THEME_COLLECTIONS,
        BottomSheetType.SINGLE_THEME_COLLECTION,
        BottomSheetType.UPLOAD_IMAGE,
        BottomSheetType.CHROME_DEFAULT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BottomSheetType {
        int MAIN = 0;
        int NTP_CARDS = 1;
        int FEED = 2;
        int THEME = 3;
        int MVT = 4;
        int THEME_COLLECTIONS = 5;
        int SINGLE_THEME_COLLECTION = 6;
        int CHROME_COLORS = 7;
        int UPLOAD_IMAGE = 8; // No dedicated bottom sheet for upload image.
        int CHROME_DEFAULT = 9; // No bottom sheet is shown.
        int NUM_ENTRIES = 10;
    }

    /**
     * New Tab Page Customization bottom sheet entry point.
     *
     * <p>These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused. See tools/metrics/histograms/enums.xml.
     */
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
    NtpCustomizationCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            Supplier<@Nullable Profile> profileSupplier,
            @BottomSheetType int bottomSheetType) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mProfileSupplier = profileSupplier;
        mBottomSheetType = bottomSheetType;
        View contentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.ntp_customization_bottom_sheet, /* root= */ null);
        mViewFlipperView = contentView.findViewById(R.id.ntp_customization_view_flipper);

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
                        context,
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
            if (ChromeFeatureList.sNewTabPageCustomizationForMvt.isEnabled()) {
                mMediator.registerClickListener(MVT, getOptionClickListener(MVT));
            }
            mMediator.registerClickListener(NTP_CARDS, getOptionClickListener(NTP_CARDS));
            mMediator.registerClickListener(FEED, getOptionClickListener(FEED));
            mMediator.registerClickListener(THEME, getOptionClickListener(THEME));
            mMediator.renderListContent();
        }
    }

    @VisibleForTesting
    NtpCustomizationBottomSheetContent initBottomSheetContent(View contentView) {
        return new NtpCustomizationBottomSheetContent(
                contentView,
                () -> mBottomSheetController.getContainerHeight(),
                () -> mBottomSheetController.getMaxSheetWidth(),
                mBottomSheetType == MAIN
                        ? () -> mMediator.backPressOnCurrentBottomSheet()
                        : () -> mMediator.dismissBottomSheet(/* animate= */ true),
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
            case THEME -> showThemeBottomSheet();
            default -> {
                assert false : "Bottom sheet type not supported!";
            }
        }
    }

    private void showNtpCardsBottomSheet() {
        if (mNtpCardsCoordinator == null) {
            mNtpCardsCoordinator = new NtpCardsCoordinator(mContext, mDelegate, mProfileSupplier);
        }
        mMediator.showBottomSheet(NTP_CARDS);
    }

    private void showFeedBottomSheet() {
        if (mFeedSettingsCoordinator == null) {
            Profile profile = assumeNonNull(mProfileSupplier.get());
            mFeedSettingsCoordinator =
                    new FeedSettingsCoordinator(mContext, mDelegate, profile.getOriginalProfile());
        }
        mMediator.showBottomSheet(FEED);
    }

    private void showMvtSettingCoordinator() {
        if (mMvtSettingCoordinator == null) {
            mMvtSettingCoordinator = new MvtSettingsCoordinator(mContext, mDelegate);
        }
        mMediator.showBottomSheet(MVT);
    }

    private void showThemeBottomSheet() {
        if (mNtpThemeCoordinator == null) {
            Profile profile = assumeNonNull(mProfileSupplier.get());
            mNtpThemeCoordinator =
                    new NtpThemeCoordinator(
                            mContext,
                            mDelegate,
                            profile.getOriginalProfile(),
                            () -> mMediator.dismissBottomSheet(/* animate= */ false));
        }
        mMediator.showBottomSheet(THEME);
    }

    void dismissBottomSheet() {
        mMediator.dismissBottomSheet(/* animate= */ true);
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
            case THEME -> {
                return v -> showThemeBottomSheet();
            }
            case MVT -> {
                return v -> showMvtSettingCoordinator();
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

            @Override
            public void showBottomSheet(@BottomSheetType int type) {
                mMediator.showBottomSheet(type);

                // When redirecting to the single theme collection bottom sheet and theme
                // collections bottom sheet, the bottom sheet needs to be updated to reflect the
                // latest selections.
                if ((type == THEME_COLLECTIONS || type == BottomSheetType.SINGLE_THEME_COLLECTION)
                        && mNtpThemeCoordinator != null) {
                    mNtpThemeCoordinator.initializeBottomSheetContent(type);
                }
            }

            @Override
            public BottomSheetController getBottomSheetController() {
                return mBottomSheetController;
            }

            @Override
            public void onNewColorSelected(boolean isDifferentColor) {
                mMediator.onNewColorSelected(isDifferentColor);
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
        NtpCustomizationCoordinatorFactory.getInstance()
                .onNtpCustomizationCoordinatorDestroyed(this);

        if (mMvtSettingCoordinator != null) {
            mMvtSettingCoordinator.destroy();
        }
        if (mNtpCardsCoordinator != null) {
            mNtpCardsCoordinator.destroy();
        }
        if (mFeedSettingsCoordinator != null) {
            mFeedSettingsCoordinator.destroy();
        }
        if (mNtpThemeCoordinator != null) {
            mNtpThemeCoordinator.destroy();
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

    void setNtpThemeCoordinatorForTesting(NtpThemeCoordinator coordinator) {
        mNtpThemeCoordinator = coordinator;
    }
}
