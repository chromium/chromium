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
    /**
     * Binds the given {@link PropertyModel} to the given {@link View} for the given {@link
     * PropertyKey}.
     */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        view = ActionButtonBinder.resolveView(view);
        assert view instanceof TabSwitcherButtonView : "View must be TabSwitcherButtonView";
        TabSwitcherButtonView tabSwitcherButtonView = (TabSwitcherButtonView) view;

        if (propertyKey == TabSwitcherActionProperties.TAB_COUNT
                || propertyKey == TabSwitcherActionProperties.IS_INCOGNITO) {
            tabSwitcherButtonView.setTabCount(
                    model.get(TabSwitcherActionProperties.TAB_COUNT),
                    model.get(TabSwitcherActionProperties.IS_INCOGNITO));
        } else if (propertyKey == TabSwitcherActionProperties.HAS_NOTIFICATION_DOT) {
            tabSwitcherButtonView.setNotificationDotVisible(
                    model.get(TabSwitcherActionProperties.HAS_NOTIFICATION_DOT));
        } else if (propertyKey == TabSwitcherActionProperties.SHOW_TAB_SWITCHER_TRIGGER) {
            tabSwitcherButtonView.endRippleAnimation();
        } else {
            ActionButtonBinder.bind(model, view, propertyKey);
        }
    }
}
