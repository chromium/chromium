// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.DeclutterMessageCardViewProperties.ARCHIVED_TABS_EXPAND_CLICK_HANDLER;
import static org.chromium.chrome.browser.tasks.tab_management.DeclutterMessageCardViewProperties.ARCHIVED_TAB_COUNT;
import static org.chromium.chrome.browser.tasks.tab_management.DeclutterMessageCardViewProperties.DECLUTTER_INFO_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.DeclutterMessageCardViewProperties.DECLUTTER_SETTINGS_CLICK_HANDLER;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the DeclutterMessageCard layout. */
public class DeclutterMessageCardViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        assert view instanceof DeclutterMessageCardView;

        DeclutterMessageCardView itemView = (DeclutterMessageCardView) view;
        if (propertyKey == DECLUTTER_INFO_TEXT || propertyKey == ARCHIVED_TAB_COUNT) {
            itemView.setDescriptionText(
                    itemView.getContext()
                            .getResources()
                            .getQuantityString(
                                    model.get(DECLUTTER_INFO_TEXT),
                                    model.get(ARCHIVED_TAB_COUNT),
                                    model.get(ARCHIVED_TAB_COUNT)));
        } else if (propertyKey == DECLUTTER_SETTINGS_CLICK_HANDLER) {
            itemView.setSettingsButtonOnClickListener(
                    (v) -> model.get(DECLUTTER_SETTINGS_CLICK_HANDLER).run());
        } else if (propertyKey == ARCHIVED_TABS_EXPAND_CLICK_HANDLER) {
            itemView.setExpandButtonOnClickListener(
                    (v) -> model.get(ARCHIVED_TABS_EXPAND_CLICK_HANDLER).run());
        }
    }
}
