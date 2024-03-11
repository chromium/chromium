// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.COLOR_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.CREATION_MILLIS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.OPEN_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.START_DRAWABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.TITLE_DATA;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/** Forwards changed property values to the view. */
public class TabGroupRowViewBinder
        implements ViewBinder<PropertyModel, TabGroupRowView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, TabGroupRowView view, PropertyKey propertyKey) {
        if (propertyKey == START_DRAWABLE) {
            view.setStartImage(model.get(START_DRAWABLE));
        } else if (propertyKey == COLOR_INDEX) {
            view.setColorIndex(model.get(COLOR_INDEX));
        } else if (propertyKey == TITLE_DATA) {
            view.setTitleData(model.get(TITLE_DATA));
        } else if (propertyKey == CREATION_MILLIS) {
            view.setCreationMillis(model.get(CREATION_MILLIS));
        } else if (propertyKey == OPEN_RUNNABLE) {
            view.setOpenRunnable(model.get(OPEN_RUNNABLE));
        } else if (propertyKey == DELETE_RUNNABLE) {
            view.setDeleteRunnable(model.get(DELETE_RUNNABLE));
        }
    }
}
