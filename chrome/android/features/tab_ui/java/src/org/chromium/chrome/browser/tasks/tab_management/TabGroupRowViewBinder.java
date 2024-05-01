// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_LEFT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_RIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_TOP_LEFT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_TOP_RIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.COLOR_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.CREATION_MILLIS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.OPEN_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.PLUS_COUNT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.TITLE_DATA;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.AsyncDrawable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/** Forwards changed property values to the view. */
public class TabGroupRowViewBinder
        implements ViewBinder<PropertyModel, TabGroupRowView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, TabGroupRowView view, PropertyKey propertyKey) {
        if (propertyKey == ASYNC_FAVICON_TOP_LEFT) {
            proxyFavicon(model, view, ASYNC_FAVICON_TOP_LEFT, /* plusCount= */ 0, Corner.TOP_LEFT);
        } else if (propertyKey == ASYNC_FAVICON_TOP_RIGHT) {
            proxyFavicon(
                    model, view, ASYNC_FAVICON_TOP_RIGHT, /* plusCount= */ 0, Corner.TOP_RIGHT);
        } else if (propertyKey == ASYNC_FAVICON_BOTTOM_LEFT) {
            proxyFavicon(
                    model, view, ASYNC_FAVICON_BOTTOM_LEFT, /* plusCount= */ 0, Corner.BOTTOM_LEFT);
        } else if (propertyKey == ASYNC_FAVICON_BOTTOM_RIGHT || propertyKey == PLUS_COUNT) {
            @Nullable Integer count = model.get(PLUS_COUNT);
            int intCount = count == null ? 0 : count.intValue();
            if (model.get(ASYNC_FAVICON_BOTTOM_RIGHT) == null) {
                view.setFavicon(null, intCount, Corner.BOTTOM_RIGHT);
            } else {
                proxyFavicon(
                        model, view, ASYNC_FAVICON_BOTTOM_RIGHT, intCount, Corner.BOTTOM_RIGHT);
            }
        } else if (propertyKey == COLOR_INDEX) {
            view.setColorIndex(model.get(COLOR_INDEX));
        } else if (propertyKey == TITLE_DATA) {
            view.setTitleData(model.get(TITLE_DATA));
        } else if (propertyKey == CREATION_MILLIS) {
            view.setCreationMillis(model.get(CREATION_MILLIS));
        } else if (propertyKey == OPEN_RUNNABLE || propertyKey == DELETE_RUNNABLE) {
            view.setMenuRunnables(model.get(OPEN_RUNNABLE), model.get(DELETE_RUNNABLE));
        }
    }

    private void proxyFavicon(
            PropertyModel model,
            TabGroupRowView view,
            WritableObjectPropertyKey<AsyncDrawable> propertyKey,
            int plusCount,
            @Corner int corner) {
        @Nullable AsyncDrawable asyncDrawable = model.get(propertyKey);
        if (asyncDrawable == null) {
            view.setFavicon(null, plusCount, corner);
        } else {
            asyncDrawable.accept(drawable -> view.setFavicon(drawable, plusCount, corner));
        }
    }
}
