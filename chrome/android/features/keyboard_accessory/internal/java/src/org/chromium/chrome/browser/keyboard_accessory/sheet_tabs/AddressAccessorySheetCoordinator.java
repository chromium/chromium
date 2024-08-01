// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabProperties.ITEMS;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.autofill.helpers.FaviconHelper;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece.Type;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

/**
 * This component is a tab that can be added to the ManualFillingCoordinator which shows it as
 * bottom sheet below the keyboard accessory.
 */
public class AddressAccessorySheetCoordinator extends AccessorySheetTabCoordinator {
    private final AccessorySheetTabMediator mMediator;
    private final Profile mProfile;

    /**
     * Creates the address tab.
     *
     * @param context The {@link Context} containing resources like icons and layouts for this tab.
     * @param scrollListener An optional listener that will be bound to the inflated recycler view.
     */
    public AddressAccessorySheetCoordinator(
            Context context,
            Profile profile,
            @Nullable RecyclerView.OnScrollListener scrollListener) {
        super(
                context.getString(R.string.address_accessory_sheet_title),
                IconProvider.getIcon(context, R.drawable.gm_filled_location_on_24),
                context.getString(R.string.address_accessory_sheet_toggle),
                R.layout.address_accessory_sheet,
                AccessoryTabType.ADDRESSES,
                scrollListener);
        mProfile = profile;
        mMediator =
                new AccessorySheetTabMediator(
                        mModel,
                        AccessoryTabType.ADDRESSES,
                        Type.ADDRESS_INFO,
                        AccessoryAction.MANAGE_ADDRESSES,
                        null);
    }

    @Override
    public void onTabCreated(ViewGroup view) {
        super.onTabCreated(view);
        FaviconHelper faviconHelper = FaviconHelper.create(view.getContext(), mProfile);
        AddressAccessorySheetViewBinder.initializeView(
                (RecyclerView) view, mModel.get(ITEMS), faviconHelper);
    }

    @Override
    protected AccessorySheetTabMediator getMediator() {
        return mMediator;
    }

    /**
     * Creates an adapter to an {@link AddressAccessorySheetViewBinder} that is wired up to a model
     * change processor listening to the {@link AccessorySheetTabItemsModel}.
     *
     * @param model the {@link AccessorySheetTabItemsModel} the adapter gets its data from.
     * @return Returns a fully initialized and wired adapter to a AddressAccessorySheetViewBinder.
     */
    static RecyclerViewAdapter<AccessorySheetTabViewBinder.ElementViewHolder, Void> createAdapter(
            AccessorySheetTabItemsModel model, FaviconHelper faviconHelper) {
        return new RecyclerViewAdapter<>(
                new SimpleRecyclerViewMcp<>(
                        model,
                        AccessorySheetDataPiece::getType,
                        AccessorySheetTabViewBinder.ElementViewHolder::bind),
                (parent, viewType) ->
                        AddressAccessorySheetViewBinder.create(parent, viewType, faviconHelper));
    }
}
