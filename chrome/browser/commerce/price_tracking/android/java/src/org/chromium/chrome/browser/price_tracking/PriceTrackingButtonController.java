// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Objects;

/**
 * Responsible for providing UI resources for showing price tracking action on optional toolbar
 * button.
 */
public class PriceTrackingButtonController extends BaseButtonDataProvider {

    private final SnackbarManager mSnackbarManager;
    private final Supplier<TabBookmarker> mTabBookmarkerSupplier;
    private final BottomSheetController mBottomSheetController;
    private final ObservableSupplier<Boolean> mPriceTrackingCurrentTabStateSupplier;
    private final ObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final BottomSheetObserver mBottomSheetObserver;
    private final Callback<Boolean> mPriceTrackingStateChangedCallback = this::updateButtonIcon;
    private final ButtonSpec mFilledButtonSpec;
    private final ButtonSpec mEmptyButtonSpec;
    private boolean mIsCurrentTabPriceTracked;

    /** Constructor. */
    public PriceTrackingButtonController(
            Context context,
            ObservableSupplier<Tab> tabSupplier,
            ModalDialogManager modalDialogManager,
            BottomSheetController bottomSheetController,
            SnackbarManager snackbarManager,
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            ObservableSupplier<Boolean> priceTrackingCurrentTabStateSupplier) {
        super(
                tabSupplier,
                modalDialogManager,
                AppCompatResources.getDrawable(context, R.drawable.price_tracking_disabled),
                context.getString(R.string.enable_price_tracking_menu_item),
                /* actionChipLabelResId= */ R.string.enable_price_tracking_menu_item,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.PRICE_TRACKING,
                /* tooltipTextResId= */ Resources.ID_NULL,
                /* showHoverHighlight= */ false);
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
                        /* clickListener= */ this,
                        /* longClickListener= */ null,
                        /* contentDescription= */ context.getString(
                                R.string.disable_price_tracking_menu_item),
                        /* supportsTinting= */ true,
                        /* iphCommandBuilder= */ null,
                        /* buttonVariant= */ AdaptiveToolbarButtonVariant.PRICE_TRACKING,
                        /* actionChipLabelResId= */ Resources.ID_NULL,
                        /* tooltipTextResId= */ Resources.ID_NULL,
                        /* showHoverHighlight= */ false);

        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetStateChanged(int newState, int reason) {
                        mButtonData.setEnabled(newState == SheetState.HIDDEN);
                        notifyObservers(mButtonData.canShow());
                    }
                };
        mBottomSheetController.addObserver(mBottomSheetObserver);

        mPriceTrackingCurrentTabStateSupplier.addObserver(mPriceTrackingStateChangedCallback);
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
            PowerBookmarkUtils.setPriceTrackingEnabledWithSnackbars(
                    mBookmarkModelSupplier.get(),
                    mBookmarkModelSupplier.get().getUserBookmarkIdForTab(mActiveTabSupplier.get()),
                    /* enabled= */ false,
                    mSnackbarManager,
                    view.getResources(),
                    mProfileSupplier.get(),
                    (success) -> {});
        } else {
            mTabBookmarkerSupplier.get().startOrModifyPriceTracking(mActiveTabSupplier.get());
        }
    }

    @Override
    protected IPHCommandBuilder getIphCommandBuilder(Tab tab) {
        IPHCommandBuilder iphCommandBuilder =
                new IPHCommandBuilder(
                        tab.getContext().getResources(),
                        FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_QUIET_VARIANT,
                        /* stringId= */ R.string.iph_price_tracking_menu_item,
                        /* accessibilityStringId= */ R.string.iph_price_tracking_menu_item);
        return iphCommandBuilder;
    }
}
