// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/** List of properties used by the CustomMessageCardItem. */
public class CustomMessageCardViewProperties {
    public static final ReadableObjectPropertyKey<View> CUSTOM_VIEW =
            new ReadableObjectPropertyKey<>();

    /** Binds the provider's setIsIncognito function to a property. */
    public static final ReadableObjectPropertyKey<Callback<Boolean>> IS_INCOGNITO_CALLBACK =
            new ReadableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                CUSTOM_VIEW,
                IS_INCOGNITO,
                IS_INCOGNITO_CALLBACK,
                MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE,
                CARD_ALPHA,
                CARD_TYPE,
                MESSAGE_TYPE
            };
}
