// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ANIMATION_STATUS;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** List of properties used by the archived_tab_message_card_view layout. */
@NullMarked
public class ArchivedTabsCardViewProperties {
    public static final WritableIntPropertyKey NUMBER_OF_ARCHIVED_TABS =
            new WritableIntPropertyKey();
    public static final WritableBooleanPropertyKey ICON_HIGHLIGHTED =
            new WritableBooleanPropertyKey();
    public static final ReadableObjectPropertyKey<Runnable> CLICK_HANDLER =
            new ReadableObjectPropertyKey<>();
    public static final WritableIntPropertyKey WIDTH = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                NUMBER_OF_ARCHIVED_TABS,
                ICON_HIGHLIGHTED,
                CLICK_HANDLER,
                WIDTH,
                MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE,
                CARD_ALPHA,
                CARD_ANIMATION_STATUS,
                CARD_TYPE,
                MESSAGE_TYPE
            };
}
