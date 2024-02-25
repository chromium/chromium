// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * A helper {@link ViewBinder} responsible for gluing {@link FilterProperties} to
 * {@link FilterView}.
 */
class FilterViewBinder implements ViewBinder<PropertyModel, FilterView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, FilterView view, PropertyKey propertyKey) {
        if (propertyKey == FilterProperties.CONTENT_VIEW) {
            view.setContentView(model.get(FilterProperties.CONTENT_VIEW));
        } else if (propertyKey == FilterProperties.SELECTED_TAB) {
            view.setTabSelected(model.get(FilterProperties.SELECTED_TAB));
        } else if (propertyKey == FilterProperties.CHANGE_LISTENER) {
            view.setTabSelectedCallback(model.get(FilterProperties.CHANGE_LISTENER));
        } else if (propertyKey == FilterProperties.SHOW_TABS) {
            view.setShowTabs(model.get(FilterProperties.SHOW_TABS));
        }
    }
}
