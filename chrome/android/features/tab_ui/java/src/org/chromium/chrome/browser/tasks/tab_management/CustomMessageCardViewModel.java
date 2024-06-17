// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;

import org.chromium.ui.modelutil.PropertyModel;

public class CustomMessageCardViewModel {
    public static PropertyModel create(CustomMessageCardProvider provider) {
        return new PropertyModel.Builder(CustomMessageCardViewProperties.ALL_KEYS)
                .with(CustomMessageCardViewProperties.CUSTOM_VIEW, provider.getCustomView())
                .with(
                        MessageCardViewProperties
                                .MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE,
                        MessageCardViewProperties.MessageCardScope.REGULAR)
                .with(
                        CustomMessageCardViewProperties.IS_INCOGNITO_CALLBACK,
                        provider::setIsIncognito)
                .with(CARD_ALPHA, 1f)
                .with(CARD_TYPE, TabListModel.CardProperties.ModelType.MESSAGE)
                .with(MESSAGE_TYPE, provider.getMessageType())
                .build();
    }
}
