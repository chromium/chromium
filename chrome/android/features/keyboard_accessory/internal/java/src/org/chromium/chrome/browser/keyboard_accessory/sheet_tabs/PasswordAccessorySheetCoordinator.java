// Copyright 2018 The Chromium Authors
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
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

/**
 * This component is a tab that can be added to the ManualFillingCoordinator which shows it
 * as bottom sheet below the keyboard accessory.
 */
public class PasswordAccessorySheetCoordinator extends AccessorySheetTabCoordinator {
    private final PasswordAccessorySheetMediator mMediator;
    private final Context mContext;
    private final Profile mProfile;

    /**
     * Creates the passwords tab.
     *
     * @param context The {@link Context} containing resources like icons and layouts for this tab.
     * @param profile The {@link Profile} associated with the passwords.
     * @param scrollListener An optional listener that will be bound to the inflated recycler view.
     */
    public PasswordAccessorySheetCoordinator(
            Context context,
            Profile profile,
            @Nullable RecyclerView.OnScrollListener scrollListener) {
        super(
                context.getString(R.string.password_list_title),
                IconProvider.getIcon(context, R.drawable.ic_password_manager_key),
                context.getString(R.string.password_accessory_sheet_toggle),
                R.layout.password_accessory_sheet,
                AccessoryTabType.PASSWORDS,
                scrollListener);
        mContext = context;
        mProfile = profile;
        mMediator =
                new PasswordAccessorySheetMediator(
                        mModel,
                        AccessoryTabType.PASSWORDS,
                        Type.PASSWORD_INFO,
                        AccessoryAction.MANAGE_PASSWORDS,
                        this::onToggleChanged);
    }

    @Override
    public void onTabCreated(ViewGroup view) {
        super.onTabCreated(view);
        PasswordAccessorySheetViewBinder.UiConfiguration uiConfiguration =
                new PasswordAccessorySheetViewBinder.UiConfiguration();
        uiConfiguration.faviconHelper = FaviconHelper.create(view.getContext(), mProfile);
        PasswordAccessorySheetViewBinder.initializeView(
                (RecyclerView) view, createAdapter(uiConfiguration, mModel.get(ITEMS)));
    }

    @Override
    protected AccessorySheetTabMediator getMediator() {
        return mMediator;
    }

    private void onToggleChanged(boolean enabled) {
        getTab().setIcon(
                        IconProvider.getIcon(
                                mContext,
                                enabled
                                        ? R.drawable.ic_password_manager_key
                                        : R.drawable.ic_password_manager_key_off));
    }

    /**
     * Creates an adapter to an {@link PasswordAccessorySheetViewBinder} that is wired up to the
     * model change processor which listens to the {@link AccessorySheetTabItemsModel}.
     *
     * @param uiConfiguration Additional generic UI configuration.
     * @param model the {@link AccessorySheetTabItemsModel} the adapter gets its data from.
     * @return Returns an {@link PasswordAccessorySheetViewBinder} wired to a MCP.
     */
    static RecyclerViewAdapter<AccessorySheetTabViewBinder.ElementViewHolder, Void> createAdapter(
            PasswordAccessorySheetViewBinder.UiConfiguration uiConfiguration,
            ListModel<AccessorySheetDataPiece> model) {
        assert uiConfiguration != null;
        return new RecyclerViewAdapter<>(
                new SimpleRecyclerViewMcp<>(
                        model,
                        AccessorySheetDataPiece::getType,
                        AccessorySheetTabViewBinder.ElementViewHolder::bind),
                (parent, viewType) -> {
                    return PasswordAccessorySheetViewBinder.create(
                            parent, viewType, uiConfiguration);
                });
    }
}
