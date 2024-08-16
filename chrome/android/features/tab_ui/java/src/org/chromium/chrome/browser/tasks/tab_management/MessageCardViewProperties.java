// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;

import android.view.View.OnClickListener;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** List of properties used by TabGridSecondaryItem. */
class MessageCardViewProperties {
    /** An enum interface to specify where the message card can be shown. */
    @IntDef({MessageCardScope.REGULAR, MessageCardScope.INCOGNITO, MessageCardScope.BOTH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface MessageCardScope {
        // Message card would only be shown inside regular mode.
        int REGULAR = 0;
        // Message card would only be shown inside incognito mode.
        int INCOGNITO = 1;
        // Message card would be shown in both regular and incognito mode.
        int BOTH = 2;
    }

    /** This corresponds to the {@link MessageService.MessageType}. */
    public static final PropertyModel.ReadableIntPropertyKey MESSAGE_TYPE =
            new PropertyModel.ReadableIntPropertyKey();

    // Identifier is the subtype of message. For example, the message with type PRICE_MESSAGE may
    // have the identifier PRICE_WELCOME or PRICE_ALERTS.
    public static final PropertyModel.ReadableIntPropertyKey MESSAGE_IDENTIFIER =
            new PropertyModel.ReadableIntPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<String> ACTION_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String> SECONDARY_ACTION_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<CharSequence> DESCRIPTION_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<MessageCardView.IconProvider>
            ICON_PROVIDER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<
                    MessageCardView.ReviewActionProvider>
            UI_ACTION_PROVIDER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<
                    MessageCardView.DismissActionProvider>
            UI_DISMISS_ACTION_PROVIDER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<OnClickListener>
            SECONDARY_ACTION_BUTTON_CLICK_HANDLER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<
                    MessageCardView.ReviewActionProvider>
            MESSAGE_SERVICE_ACTION_PROVIDER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<
                    MessageCardView.DismissActionProvider>
            MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER =
                    new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String>
            DISMISS_BUTTON_CONTENT_DESCRIPTION = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey SHOULD_KEEP_AFTER_REVIEW =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_CLOSE_BUTTON_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_ICON_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey ICON_WIDTH_IN_PIXELS =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey ICON_HEIGHT_IN_PIXELS =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<String> TITLE_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey VIEW_AS_ACTION_BUTTON =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey ACTION_BUTTON_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    /** By default, if nothing is specified, regular is assumed. */
    public static final PropertyModel.ReadableIntPropertyKey
            MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE =
                    new PropertyModel.ReadableIntPropertyKey();

    // TODO(crbug.com/40731056): Change to a more general property CUSTOM_INFO_OBJECT
    public static final PropertyModel.WritableObjectPropertyKey<ShoppingPersistedTabData.PriceDrop>
            PRICE_DROP = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ACTION_TEXT,
                SECONDARY_ACTION_TEXT,
                DESCRIPTION_TEXT,
                MESSAGE_TYPE,
                MESSAGE_IDENTIFIER,
                ICON_PROVIDER,
                UI_ACTION_PROVIDER,
                UI_DISMISS_ACTION_PROVIDER,
                SECONDARY_ACTION_BUTTON_CLICK_HANDLER,
                MESSAGE_SERVICE_ACTION_PROVIDER,
                MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER,
                DISMISS_BUTTON_CONTENT_DESCRIPTION,
                SHOULD_KEEP_AFTER_REVIEW,
                IS_CLOSE_BUTTON_VISIBLE,
                IS_ICON_VISIBLE,
                ICON_WIDTH_IN_PIXELS,
                ICON_HEIGHT_IN_PIXELS,
                CARD_TYPE,
                CARD_ALPHA,
                IS_INCOGNITO,
                TITLE_TEXT,
                MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE,
                PRICE_DROP,
                VIEW_AS_ACTION_BUTTON,
                ACTION_BUTTON_VISIBLE
            };
}
