// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * List of properties used by TabGridSecondaryItem.
 */
class MessageCardViewProperties {
    public static PropertyModel.ReadableIntPropertyKey MESSAGE_TYPE =
            new PropertyModel.ReadableIntPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<String> ACTION_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String> DESCRIPTION_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String> DESCRIPTION_TEXT_TEMPLATE =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static PropertyModel
            .WritableObjectPropertyKey<MessageCardView.IconProvider> ICON_PROVIDER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static PropertyModel
            .WritableObjectPropertyKey<MessageCardView.ReviewActionProvider> UI_ACTION_PROVIDER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static PropertyModel.WritableObjectPropertyKey<MessageCardView.DismissActionProvider>
            UI_DISMISS_ACTION_PROVIDER = new PropertyModel.WritableObjectPropertyKey<>();
    public static PropertyModel.WritableObjectPropertyKey<MessageCardView.ReviewActionProvider>
            MESSAGE_SERVICE_ACTION_PROVIDER = new PropertyModel.WritableObjectPropertyKey<>();
    public static PropertyModel.WritableObjectPropertyKey<MessageCardView.DismissActionProvider>
            MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<String> DISMISS_BUTTON_CONTENT_DESCRIPTION =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {ACTION_TEXT, DESCRIPTION_TEXT,
            DESCRIPTION_TEXT_TEMPLATE, MESSAGE_TYPE, ICON_PROVIDER, UI_ACTION_PROVIDER,
            UI_DISMISS_ACTION_PROVIDER, MESSAGE_SERVICE_ACTION_PROVIDER,
            MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER, DISMISS_BUTTON_CONTENT_DESCRIPTION};
}