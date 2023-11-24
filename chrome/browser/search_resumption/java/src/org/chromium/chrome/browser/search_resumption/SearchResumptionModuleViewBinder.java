// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

class SearchResumptionModuleViewBinder implements ViewBinder<PropertyModel, View, PropertyKey> {
    @Override
    public final void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        SearchResumptionModuleView moduleView = (SearchResumptionModuleView) view;

        if (SearchResumptionModuleProperties.IS_VISIBLE == propertyKey) {
            moduleView.setVisibility(
                    model.get(SearchResumptionModuleProperties.IS_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (SearchResumptionModuleProperties.EXPAND_COLLAPSE_CLICK_CALLBACK == propertyKey) {
            moduleView.setExpandCollapseCallback(
                    model.get(SearchResumptionModuleProperties.EXPAND_COLLAPSE_CLICK_CALLBACK));
        } else {
            assert false : "Unhandled property detected in SearchResumptionModuleViewBinder!";
        }
    }
}
