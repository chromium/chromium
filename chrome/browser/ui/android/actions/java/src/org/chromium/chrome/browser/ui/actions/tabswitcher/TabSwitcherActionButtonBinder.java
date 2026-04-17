// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.tabswitcher;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab_ui.TabSwitcherButtonView;
import org.chromium.chrome.browser.ui.actions.ActionButtonBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds the Tab Switcher action state to a {@link TabSwitcherButtonView}. */
@NullMarked
public class TabSwitcherActionButtonBinder {
    public static void bind(
            PropertyModel model, TabSwitcherButtonView view, PropertyKey propertyKey) {
        if (propertyKey == TabSwitcherActionProperties.TAB_COUNT
                || propertyKey == TabSwitcherActionProperties.IS_INCOGNITO) {
            view.setTabCount(
                    model.get(TabSwitcherActionProperties.TAB_COUNT),
                    model.get(TabSwitcherActionProperties.IS_INCOGNITO));
        } else if (propertyKey == TabSwitcherActionProperties.HAS_NOTIFICATION_DOT) {
            view.setNotificationDotVisible(
                    model.get(TabSwitcherActionProperties.HAS_NOTIFICATION_DOT));
        } else if (propertyKey == TabSwitcherActionProperties.SHOW_TAB_SWITCHER_TRIGGER) {
            view.endRippleAnimation();
        } else {
            ActionButtonBinder.bind(model, (View) view, propertyKey);
        }
    }
}
