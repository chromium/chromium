// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** List of properties used by the DeclutterMessageCard layout. */
public class DeclutterMessageCardViewProperties {
    /** The string id of the info text shown on the message card. */
    public static final WritableIntPropertyKey DECLUTTER_INFO_TEXT = new WritableIntPropertyKey();

    /** The tab count of all tabs in the archived tab model. */
    public static final WritableIntPropertyKey ARCHIVED_TAB_COUNT = new WritableIntPropertyKey();

    /** The handler for the settings icon on the declutter message card. */
    public static final WritableObjectPropertyKey<Runnable> DECLUTTER_SETTINGS_CLICK_HANDLER =
            new WritableObjectPropertyKey<>();

    /** The handler for the expand icon when accessing the archived tabs interface. */
    public static final WritableObjectPropertyKey<Runnable> ARCHIVED_TABS_EXPAND_CLICK_HANDLER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                DECLUTTER_INFO_TEXT,
                ARCHIVED_TAB_COUNT,
                DECLUTTER_SETTINGS_CLICK_HANDLER,
                ARCHIVED_TABS_EXPAND_CLICK_HANDLER
            };
}
