// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class TabResumptionModuleViewBinder {
    public static final void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        TabResumptionModuleView moduleView = (TabResumptionModuleView) view;

        if (TabResumptionModuleProperties.IS_VISIBLE == propertyKey) {
            moduleView.setVisibility(
                    model.get(TabResumptionModuleProperties.IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (TabResumptionModuleProperties.URL_IMAGE_PROVIDER == propertyKey) {
            moduleView.setUrlImageProvider(
                    model.get(TabResumptionModuleProperties.URL_IMAGE_PROVIDER));
        } else if (TabResumptionModuleProperties.SEE_MORE_LINK_CLICK_CALLBACK == propertyKey) {
            moduleView.setSeeMoreLinkClickCallback(
                    model.get(TabResumptionModuleProperties.SEE_MORE_LINK_CLICK_CALLBACK));
        } else if (TabResumptionModuleProperties.CLICK_CALLBACK == propertyKey) {
            moduleView.setClickCallback(model.get(TabResumptionModuleProperties.CLICK_CALLBACK));
        } else if (TabResumptionModuleProperties.SUGGESTION_BUNDLE == propertyKey) {
            moduleView.setSuggestionBundle(
                    model.get(TabResumptionModuleProperties.SUGGESTION_BUNDLE));
        } else if (TabResumptionModuleProperties.TITLE == propertyKey) {
            moduleView.setTitle(model.get(TabResumptionModuleProperties.TITLE));
        } else if (TabResumptionModuleProperties.TAB_MODEL_SELECTOR_SUPPLIER == propertyKey) {
            moduleView.setTabModelSelectorSupplier(
                    model.get(TabResumptionModuleProperties.TAB_MODEL_SELECTOR_SUPPLIER));
        } else if (TabResumptionModuleProperties.TRACKING_TAB == propertyKey) {
            moduleView.setTrackingTab(model.get(TabResumptionModuleProperties.TRACKING_TAB));
        } else if (TabResumptionModuleProperties.TAB_OBSERVER_CALLBACK == propertyKey) {
            moduleView.setTabObserverCallback(
                    model.get(TabResumptionModuleProperties.TAB_OBSERVER_CALLBACK));
        } else if (TabResumptionModuleProperties.ON_MODULE_SHOW_CONFIG_FINALIZED_CALLBACK
                == propertyKey) {
            moduleView.setOnModuleShowConfigFinalizedCallback(
                    model.get(
                            TabResumptionModuleProperties
                                    .ON_MODULE_SHOW_CONFIG_FINALIZED_CALLBACK));
        } else {
            assert false : "Unhandled property detected in TabResumptionModuleViewBinder!";
        }
    }
}
