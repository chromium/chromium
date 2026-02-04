// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Objects;
import java.util.function.Supplier;

/**
 * Responsible for providing UI resources for showing price tracking action on optional toolbar
 * button.
 */
@NullMarked
public class PriceTrackingButtonController extends BaseButtonDataProvider {

    private final SnackbarManager mSnackbarManager;
    private final Supplier<TabBookmarker> mTabBookmarkerSupplier;
    private final BottomSheetController mBottomSheetController;
    private final NonNullObservableSupplier<Boolean> mPriceTrackingCurrentTabStateSupplier;
    private final NullableObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final BottomSheetObserver mBottomSheetObserver;
    private final Callback<Boolean> mPriceTrackingStateChangedCallback = this::updateButtonIcon;
    private final ButtonSpec mFilledButtonSpec;
    private final ButtonSpec mEmptyButtonSpec;
    private boolean mIsCurrentTabPriceTracked;

    /** Constructor. */
    public PriceTrackingButtonController(
            Context context,
            Supplier<@Nullable Tab> tabSupplier,
            ModalDialogManager modalDialogManager,
            BottomSheetController bottomSheetController,
            SnackbarManager snackbarManager,
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            MonotonicObservableSupplier<Profile> profileSupplier,
            NullableObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            NonNullObservableSupplier<Boolean> priceTrackingCurrentTabStateSupplier) {
        super(
                tabSupplier,
                modalDialogManager,
                AppCompatResources.getDrawable(context, R.drawable.price_tracking_disabled),
                context.getString(R.string.enable_price_tracking_menu_item),
                /* actionChipLabelResId= */ R.string.enable_price_tracking_menu_item,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.PRICE_TRACKING,
                /* tooltipTextResId= */ Resources.ID_NULL);
        mSnackbarManager = snackbarManager;
        mTabBookmarkerSupplier = tabBookmarkerSupplier;
        mBottomSheetController = bottomSheetController;
        mBookmarkModelSupplier = bookmarkModelSupplier;
        mPriceTrackingCurrentTabStateSupplier = priceTrackingCurrentTabStateSupplier;
        mProfileSupplier = profileSupplier;

        mEmptyButtonSpec = mButtonData.getButtonSpec();
        // Create another ButtonSpec with a filled price tracking icon and a "Stop tracking"
        // description.
        mFilledButtonSpec =
                new ButtonSpec(
                        /* drawable= */ AppCompatResources.getDrawable(
                                context, R.drawable.price_tracking_enabled_filled),
                        /* onClickListener= */ this,
                        /* onLongClickListener= */ null,
                        /* contentDescription= */ context.getString(
                                R.string.disable_price_tracking_menu_item),
                        /* supportsTinting= */ true,
                        /* iphCommandBuilder= */ null,
                        /* buttonVariant= */ AdaptiveToolbarButtonVariant.PRICE_TRACKING,
                        /* actionChipLabelResId= */ Resources.ID_NULL,
                        /* tooltipTextResId= */ Resources.ID_NULL,
                        /* hasErrorBadge= */ false,
                        /* isChecked= */ true);

        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetStateChanged(int newState, int reason) {
                        mButtonData.setEnabled(newState == SheetState.HIDDEN);
                        notifyObservers(mButtonData.canShow());
                    }
                };
        mBottomSheetController.addObserver(mBottomSheetObserver);

        mPriceTrackingCurrentTabStateSupplier.addSyncObserver(mPriceTrackingStateChangedCallback);
    }

    private void updateButtonIcon(boolean isCurrentTabPriceTracked) {
        mIsCurrentTabPriceTracked = isCurrentTabPriceTracked;
        ButtonSpec buttonSpecForCurrentTab =
                isCurrentTabPriceTracked ? mFilledButtonSpec : mEmptyButtonSpec;
        if (!Objects.equals(mButtonData.getButtonSpec(), buttonSpecForCurrentTab)) {
            mButtonData.setButtonSpec(buttonSpecForCurrentTab);
            notifyObservers(mButtonData.canShow());
        }
    }

    @Override
    public void destroy() {
        super.destroy();

        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mPriceTrackingCurrentTabStateSupplier.removeObserver(mPriceTrackingStateChangedCallback);
    }

    @Override
    public void onClick(View view) {
        if (mIsCurrentTabPriceTracked) {
            Profile profile = assertNonNull(mProfileSupplier.get());
            PowerBookmarkUtils.setPriceTrackingEnabledWithSnackbars(
                    assumeNonNull(mBookmarkModelSupplier.get())
                            .getUserBookmarkIdForTab(assertNonNull(mActiveTabSupplier.get())),
                    /* enabled= */ false,
                    mSnackbarManager,
                    view.getResources(),
                    profile,
                    (success) -> {},
                    PriceDropNotificationManagerFactory.create(profile));
        } else {
            mTabBookmarkerSupplier.get().startOrModifyPriceTracking(mActiveTabSupplier.get());
        }
    }

    @Override
    protected IphCommandBuilder getIphCommandBuilder(Tab tab) {
        IphCommandBuilder iphCommandBuilder =
                new IphCommandBuilder(
                        tab.getContext().getResources(),
                        FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_QUIET_VARIANT,
                        /* stringId= */ R.string.iph_price_tracking_menu_item,
                        /* accessibilityStringId= */ R.string.iph_price_tracking_menu_item);
        return iphCommandBuilder;
    }
}
