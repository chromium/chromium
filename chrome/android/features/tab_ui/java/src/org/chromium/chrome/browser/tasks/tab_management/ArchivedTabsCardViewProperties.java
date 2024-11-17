// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** List of properties used by the archived_tab_message_card_view layout. */
public class ArchivedTabsCardViewProperties {
    public static final WritableIntPropertyKey NUMBER_OF_ARCHIVED_TABS =
            new WritableIntPropertyKey();
    public static final WritableIntPropertyKey ARCHIVE_TIME_DELTA_DAYS =
            new WritableIntPropertyKey();
    public static final ReadableObjectPropertyKey<Runnable> CLICK_HANDLER =
            new ReadableObjectPropertyKey<>();
    public static final WritableIntPropertyKey WIDTH = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                NUMBER_OF_ARCHIVED_TABS, ARCHIVE_TIME_DELTA_DAYS, CLICK_HANDLER, WIDTH
            };
}
