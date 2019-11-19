// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece.Type;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

/**
 * This component is a tab that can be added to the ManualFillingCoordinator which shows it
 * as bottom sheet below the keyboard accessory.
 */
public class PasswordAccessorySheetCoordinator extends AccessorySheetTabCoordinator {
    private final AccessorySheetTabModel mModel = new AccessorySheetTabModel();
    private final AccessorySheetTabMediator mMediator = new AccessorySheetTabMediator(mModel,
            AccessoryTabType.PASSWORDS, Type.PASSWORD_INFO, AccessoryAction.MANAGE_PASSWORDS);

    /**
     * Creates the passwords tab.
     * @param context The {@link Context} containing resources like icons and layouts for this tab.
     * @param scrollListener An optional listener that will be bound to the inflated recycler view.
     */
    public PasswordAccessorySheetCoordinator(
            Context context, @Nullable RecyclerView.OnScrollListener scrollListener) {
        super(context.getString(R.string.prefs_saved_passwords_title),
                IconProvider.getIcon(context, R.drawable.ic_vpn_key_grey),
                context.getString(R.string.password_accessory_sheet_toggle),
                context.getString(R.string.password_accessory_sheet_opened),
                R.layout.password_accessory_sheet, AccessoryTabType.PASSWORDS, scrollListener);
    }

    @Override
    public void onTabCreated(ViewGroup view) {
        super.onTabCreated(view);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)) {
            PasswordAccessorySheetModernViewBinder.initializeView((RecyclerView) view, mModel);
        } else {
            PasswordAccessorySheetViewBinder.initializeView((RecyclerView) view, mModel);
        }
    }

    @Override
    protected AccessorySheetTabMediator getMediator() {
        return mMediator;
    }

    /**
     * Creates an adapter to an {@link PasswordAccessorySheetViewBinder} that is wired
     * up to a model change processor listening to the {@link AccessorySheetTabModel}.
     * @param model the {@link AccessorySheetTabModel} the adapter gets its data from.
     * @return Returns a fully initialized and wired adapter to a PasswordAccessorySheetViewBinder.
     */
    static RecyclerViewAdapter<AccessorySheetTabViewBinder.ElementViewHolder, Void> createAdapter(
            AccessorySheetTabModel model) {
        return new RecyclerViewAdapter<>(
                new SimpleRecyclerViewMcp<>(model, AccessorySheetDataPiece::getType,
                        AccessorySheetTabViewBinder.ElementViewHolder::bind),
                PasswordAccessorySheetViewBinder::create);
    }

    /**
     * Creates an adapter to an {@link PasswordAccessorySheetModernViewBinder} that is wired up to
     * the model change processor which listens to the {@link AccessorySheetTabModel}.
     * @param model the {@link AccessorySheetTabModel} the adapter gets its data from.
     * @return Returns an {@link PasswordAccessorySheetModernViewBinder} wired to a MCP.
     */
    static RecyclerViewAdapter<AccessorySheetTabViewBinder.ElementViewHolder, Void>
    createModernAdapter(ListModel<AccessorySheetDataPiece> model) {
        return new RecyclerViewAdapter<>(
                new SimpleRecyclerViewMcp<>(model, AccessorySheetDataPiece::getType,
                        AccessorySheetTabViewBinder.ElementViewHolder::bind),
                PasswordAccessorySheetModernViewBinder::create);
    }

    @VisibleForTesting
    AccessorySheetTabModel getSheetDataPiecesForTesting() {
        return mModel;
    }
}
