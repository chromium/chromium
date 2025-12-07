// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherPaneDrawableProperties.SHOW_NOTIFICATION_DOT;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherPaneDrawableProperties.TAB_COUNT;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the {@link TabSwitcherDrawable} for the {@link TabSwitcherPane}. */
@NullMarked
public class TabSwitcherPaneDrawableViewBinder {
    /** Bind method for the property model change processor. */
    public static void bind(
            PropertyModel model, TabSwitcherDrawable drawable, PropertyKey propertyKey) {
        if (propertyKey == TAB_COUNT) {
            drawable.updateForTabCount(model.get(TAB_COUNT), /* incognito= */ false);
        } else if (propertyKey == SHOW_NOTIFICATION_DOT) {
            drawable.setNotificationIconStatus(model.get(SHOW_NOTIFICATION_DOT));
        }
    }
}
