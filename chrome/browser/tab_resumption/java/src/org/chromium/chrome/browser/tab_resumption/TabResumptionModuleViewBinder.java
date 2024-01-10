// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

class TabResumptionModuleViewBinder implements ViewBinder<PropertyModel, View, PropertyKey> {
    @Override
    public final void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        TabResumptionModuleView moduleView = (TabResumptionModuleView) view;

        if (TabResumptionModuleProperties.IS_VISIBLE == propertyKey) {
            moduleView.setVisibility(
                    model.get(TabResumptionModuleProperties.IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else {
            assert false : "Unhandled property detected in TabResumptionModuleViewBinder!";
        }
    }
}
