// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;

import android.content.Context;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * A class to create the property model for the Incognito re-auth promo card.
 */
public class IncognitoReauthPromoViewModel {
    /**
     * Create a {@link PropertyModel} for incognito re-auth promo card.
     *
     * TODO(crbug.com/1227656): Build the property model to actually design the re-auth promo card.
     *
     * @param context The {@link Context} to use.
     * @param uiDismissActionProvider The {@link MessageCardView.DismissActionProvider} to set.
     * @param data The {@link IncognitoReauthPromoMessageService.IncognitoReauthMessageData} to use.
     * @return A {@link PropertyModel} for the given {@code data}.
     */
    public static PropertyModel create(Context context,
            MessageCardView.DismissActionProvider uiDismissActionProvider,
            IncognitoReauthPromoMessageService.IncognitoReauthMessageData data) {
        return new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                .with(MessageCardViewProperties.MESSAGE_TYPE,
                        MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE)
                .with(MessageCardViewProperties
                                .MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE,
                        MessageCardViewProperties.MessageCardScope.INCOGNITO)
                .with(CARD_TYPE, MESSAGE)
                .build();
    }
}