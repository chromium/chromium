// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.tabswitcher;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties specific to the Tab Switcher action. */
@NullMarked
public class TabSwitcherActionProperties {
    public static final WritableIntPropertyKey TAB_COUNT = new WritableIntPropertyKey();
    public static final WritableBooleanPropertyKey HAS_NOTIFICATION_DOT =
            new WritableBooleanPropertyKey();

    /** Setting this property will trigger the tab switcher to show. */
    public static final WritableObjectPropertyKey<Void> SHOW_TAB_SWITCHER_TRIGGER =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);

    public static final WritableBooleanPropertyKey IS_INCOGNITO = new WritableBooleanPropertyKey();
    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(
                    ActionProperties.BASE_KEYS_WITH_BUTTON_STATE_AND_NO_ICON,
                    new PropertyKey[] {
                        TAB_COUNT, HAS_NOTIFICATION_DOT, SHOW_TAB_SWITCHER_TRIGGER, IS_INCOGNITO
                    });
}
