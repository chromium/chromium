// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getCardIcon;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabProperties.ITEMS;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece.Type;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.ImageSize;

/**
 * This component is a tab that can be added to the ManualFillingCoordinator. This tab allows
 * selecting credit card information from a sheet below the keyboard accessory.
 */
public class CreditCardAccessorySheetCoordinator extends AccessorySheetTabCoordinator {
    private final AccessorySheetTabMediator mMediator;
    private final CreditCardAccessorySheetViewBinder.UiConfiguration mUiConfiguration;

    /**
     * Creates the credit cards tab.
     *
     * @param context The {@link Context} containing resources like icons and layouts for this tab.
     * @param profile The {@link Profile} associated with the data being displayed.
     * @param scrollListener An optional listener that will be bound to the inflated recycler view.
     */
    public CreditCardAccessorySheetCoordinator(
            Context context,
            Profile profile,
            @Nullable RecyclerView.OnScrollListener scrollListener) {
        super(
                context.getString(R.string.autofill_payment_methods),
                IconProvider.getIcon(context, R.drawable.infobar_autofill_cc),
                context.getString(R.string.credit_card_accessory_sheet_toggle),
                R.layout.credit_card_accessory_sheet,
                AccessoryTabType.CREDIT_CARDS,
                scrollListener);
        mUiConfiguration =
                createUiConfiguration(context, PersonalDataManagerFactory.getForProfile(profile));
        mMediator =
                new AccessorySheetTabMediator(
                        mModel,
                        AccessoryTabType.CREDIT_CARDS,
                        Type.CREDIT_CARD_INFO,
                        AccessoryAction.MANAGE_CREDIT_CARDS,
                        null);
    }

    @Override
    protected AccessorySheetTabMediator getMediator() {
        return mMediator;
    }

    @Override
    public void onTabCreated(ViewGroup view) {
        super.onTabCreated(view);
        CreditCardAccessorySheetViewBinder.initializeView(
                (RecyclerView) view, mUiConfiguration, mModel.get(ITEMS));
    }

    @VisibleForTesting
    static CreditCardAccessorySheetViewBinder.UiConfiguration createUiConfiguration(
            Context context, PersonalDataManager personalDataManager) {
        CreditCardAccessorySheetViewBinder.UiConfiguration uiConfiguration =
                new CreditCardAccessorySheetViewBinder.UiConfiguration();
        uiConfiguration.cardDrawableFunction =
                (info) ->
                        getCardIcon(
                                context,
                                personalDataManager,
                                info.getIconUrl(),
                                CreditCardAccessorySheetViewBinder.getDrawableForOrigin(
                                        info.getOrigin()),
                                ImageSize.SMALL,
                                /* showCustomIcon= */ true);
        return uiConfiguration;
    }
}
