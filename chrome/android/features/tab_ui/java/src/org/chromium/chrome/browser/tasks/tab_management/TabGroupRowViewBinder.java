// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.CLUSTER_DATA;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.COLOR_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.CREATION_MILLIS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DISPLAY_AS_SHARED;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.GET_IMAGE_TILE_CONTAINER_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.LEAVE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.OPEN_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.TITLE_DATA;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/** Forwards changed property values to the view. */
public class TabGroupRowViewBinder
        implements ViewBinder<PropertyModel, TabGroupRowView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, TabGroupRowView view, PropertyKey propertyKey) {
        if (propertyKey == CLUSTER_DATA) {
            view.updateCornersForClusterData(model.get(CLUSTER_DATA));
        } else if (propertyKey == DISPLAY_AS_SHARED) {
            view.setDisplayAsShared(model.get(DISPLAY_AS_SHARED));
        } else if (propertyKey == COLOR_INDEX) {
            view.setColorIndex(model.get(COLOR_INDEX));
        } else if (propertyKey == TITLE_DATA) {
            view.setTitleData(model.get(TITLE_DATA));
        } else if (propertyKey == CREATION_MILLIS) {
            view.setCreationMillis(model.get(CREATION_MILLIS));
        } else if (propertyKey == OPEN_RUNNABLE
                || propertyKey == DELETE_RUNNABLE
                || propertyKey == LEAVE_RUNNABLE) {
            view.setMenuRunnables(
                    model.get(OPEN_RUNNABLE),
                    model.get(DELETE_RUNNABLE),
                    model.get(LEAVE_RUNNABLE));
        } else if (propertyKey == GET_IMAGE_TILE_CONTAINER_CALLBACK) {
            view.setGetImageTileContainerCallback(model.get(GET_IMAGE_TILE_CONTAINER_CALLBACK));
        }
    }
}
