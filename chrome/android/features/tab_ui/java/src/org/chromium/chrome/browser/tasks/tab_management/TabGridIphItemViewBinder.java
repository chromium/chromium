// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGridIphItemProperties.IPH_DIALOG_CLOSE_BUTTON_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridIphItemProperties.IPH_ENTRANCE_CLOSE_BUTTON_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridIphItemProperties.IPH_ENTRANCE_SHOW_BUTTON_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridIphItemProperties.IPH_SCRIM_VIEW_OBSERVER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridIphItemProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridIphItemProperties.IS_IPH_ENTRANCE_VISIBLE;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for TabGridIphItem.
 */
class TabGridIphItemViewBinder {
    public static class ViewHolder {
        public final TabListRecyclerView contentView;
        public final TabGridIphItemView iphItemView;
        ViewHolder(TabListRecyclerView contentView, TabGridIphItemView iphItemView) {
            this.contentView = contentView;
            this.iphItemView = iphItemView;
        }
    }

    public static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (IPH_DIALOG_CLOSE_BUTTON_LISTENER == propertyKey) {
            viewHolder.iphItemView.setupCloseIphDialogButtonOnclickListener(
                    model.get(IPH_DIALOG_CLOSE_BUTTON_LISTENER));
        } else if (IPH_ENTRANCE_CLOSE_BUTTON_LISTENER == propertyKey) {
            viewHolder.iphItemView.setupCloseIphEntranceButtonOnclickListener(
                    model.get(IPH_ENTRANCE_CLOSE_BUTTON_LISTENER));
        } else if (IPH_ENTRANCE_SHOW_BUTTON_LISTENER == propertyKey) {
            viewHolder.iphItemView.setupShowIphButtonOnclickListener(
                    model.get(IPH_ENTRANCE_SHOW_BUTTON_LISTENER));
        } else if (IPH_SCRIM_VIEW_OBSERVER == propertyKey) {
            viewHolder.iphItemView.setupIPHDialogScrimViewObserver(
                    model.get(IPH_SCRIM_VIEW_OBSERVER));
        } else if (IS_IPH_DIALOG_VISIBLE == propertyKey) {
            if (model.get(IS_IPH_DIALOG_VISIBLE)) {
                viewHolder.iphItemView.showIPHDialog();
            } else {
                viewHolder.iphItemView.closeIphDialog();
            }
        } else if (IS_IPH_ENTRANCE_VISIBLE == propertyKey) {
            if (model.get(IS_IPH_ENTRANCE_VISIBLE)) {
                viewHolder.contentView.setupRecyclerViewFooter(viewHolder.iphItemView);
            } else {
                viewHolder.contentView.removeRecyclerViewFooter();
            }
        } else if (IS_INCOGNITO == propertyKey) {
            boolean isIncognito = model.get(TabGridIphItemProperties.IS_INCOGNITO);
            viewHolder.iphItemView.updateColor(isIncognito);
        }
    }
}
