// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;

import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * List of properties used by TabGridSecondaryItem.
 */
class MessageCardViewProperties {
    public static final PropertyModel.ReadableIntPropertyKey MESSAGE_TYPE =
            new PropertyModel.ReadableIntPropertyKey();
    // Identifier is the subtype of message. For example, the message with type PRICE_MESSAGE may
    // have the identifier PRICE_WELCOME or PRICE_ALERTS.
    public static final PropertyModel.ReadableIntPropertyKey MESSAGE_IDENTIFIER =
            new PropertyModel.ReadableIntPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<String> ACTION_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String> DESCRIPTION_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String> DESCRIPTION_TEXT_TEMPLATE =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<MessageCardView.IconProvider> ICON_PROVIDER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<MessageCardView.ReviewActionProvider> UI_ACTION_PROVIDER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<MessageCardView.DismissActionProvider>
                    UI_DISMISS_ACTION_PROVIDER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<
            MessageCardView.ReviewActionProvider> MESSAGE_SERVICE_ACTION_PROVIDER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<
            MessageCardView.DismissActionProvider> MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<String> DISMISS_BUTTON_CONTENT_DESCRIPTION =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey SHOULD_KEEP_AFTER_REVIEW =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_ICON_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<String> TITLE_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey SHOULD_SHOW_IN_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();
    // TODO(crbug.com/1148020): Change to a more general property CUSTOM_INFO_OBJECT
    public static final PropertyModel
            .WritableObjectPropertyKey<ShoppingPersistedTabData.PriceDrop> PRICE_DROP =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {ACTION_TEXT, DESCRIPTION_TEXT,
            DESCRIPTION_TEXT_TEMPLATE, MESSAGE_TYPE, MESSAGE_IDENTIFIER, ICON_PROVIDER,
            UI_ACTION_PROVIDER, UI_DISMISS_ACTION_PROVIDER, MESSAGE_SERVICE_ACTION_PROVIDER,
            MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER, DISMISS_BUTTON_CONTENT_DESCRIPTION,
            SHOULD_KEEP_AFTER_REVIEW, IS_ICON_VISIBLE, CARD_TYPE, CARD_ALPHA, IS_INCOGNITO,
            TITLE_TEXT, SHOULD_SHOW_IN_INCOGNITO, PRICE_DROP};
}
