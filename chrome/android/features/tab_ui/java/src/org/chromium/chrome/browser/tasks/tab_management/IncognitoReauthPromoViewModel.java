// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;

import android.content.Context;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;

/** A class to create the property model for the Incognito re-auth promo card. */
@NullMarked
public class IncognitoReauthPromoViewModel {
    /**
     * Create a {@link PropertyModel} for incognito re-auth promo card.
     *
     * @param context The {@link Context} to use.
     * @param msgServiceDismissActionProvider The {@link ServiceDismissActionProvider} to set.
     * @param data The {@link IncognitoReauthPromoMessageService.IncognitoReauthMessageData} to use.
     * @return A {@link PropertyModel} for the given {@code data}.
     */
    public static PropertyModel create(
            Context context,
            ServiceDismissActionProvider<@MessageType Integer> msgServiceDismissActionProvider,
            IncognitoReauthPromoMessageService.IncognitoReauthMessageData data) {
        String titleText =
                IncognitoUtils.shouldOpenIncognitoAsWindow()
                        ? context.getString(R.string.incognito_window_reauth_promo_title)
                        : context.getString(R.string.incognito_reauth_promo_title);
        String descriptionText =
                IncognitoUtils.shouldOpenIncognitoAsWindow()
                        ? context.getString(R.string.incognito_window_reauth_promo_description)
                        : context.getString(R.string.incognito_reauth_promo_description);
        String actionText = context.getString(R.string.incognito_reauth_lock_action_text);
        String dismissActionText = context.getString(R.string.no_thanks);

        return new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                .with(
                        MessageCardViewProperties.MESSAGE_TYPE,
                        MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE)
                .with(
                        MessageCardViewProperties
                                .MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE,
                        MessageCardViewProperties.MessageCardScope.INCOGNITO)
                .with(
                        MessageCardViewProperties.MESSAGE_IDENTIFIER,
                        MessageService.DEFAULT_MESSAGE_IDENTIFIER)
                .with(
                        MessageCardViewProperties.MESSAGE_TYPE,
                        MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE)
                .with(MessageCardViewProperties.ACTION_TEXT, actionText)
                .with(MessageCardViewProperties.UI_ACTION_PROVIDER, data.getAcceptActionProvider())
                .with(MessageCardViewProperties.DESCRIPTION_TEXT, descriptionText)
                .with(MessageCardViewProperties.SECONDARY_ACTION_TEXT, dismissActionText)
                .with(
                        MessageCardViewProperties.SECONDARY_ACTION_BUTTON_CLICK_HANDLER,
                        view -> {
                            data.getDismissActionProvider().action();
                            RecordHistogram.recordEnumeratedHistogram(
                                    "Android.IncognitoReauth.PromoAcceptedOrDismissed",
                                    IncognitoReauthPromoMessageService
                                            .IncognitoReauthPromoActionType.NO_THANKS,
                                    IncognitoReauthPromoMessageService
                                            .IncognitoReauthPromoActionType.NUM_ENTRIES);
                        })
                .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, false)
                .with(
                        MessageCardViewProperties.ICON_WIDTH_IN_PIXELS,
                        context.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.incognito_reauth_promo_message_icon_width))
                .with(
                        MessageCardViewProperties.ICON_HEIGHT_IN_PIXELS,
                        context.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.incognito_reauth_promo_message_icon_height))
                .with(MessageCardViewProperties.IS_ICON_VISIBLE, true)
                .with(MessageCardViewProperties.IS_CLOSE_BUTTON_VISIBLE, false)
                .with(MessageCardViewProperties.IS_INCOGNITO, true)
                .with(MessageCardViewProperties.TITLE_TEXT, titleText)
                .with(
                        MessageCardViewProperties.ICON_PROVIDER,
                        (callback) -> {
                            callback.onResult(
                                    AppCompatResources.getDrawable(
                                            context, R.drawable.ic_incognito_reauth_promo_icon));
                        })
                .with(CARD_TYPE, TabListModel.CardProperties.ModelType.MESSAGE)
                .with(CARD_ALPHA, 1f)
                .build();
    }
}
