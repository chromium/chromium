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
        } else if (TabResumptionModuleProperties.USE_SALIENT_IMAGE == propertyKey) {
            moduleView.setUseSalientImage(
                    model.get(TabResumptionModuleProperties.USE_SALIENT_IMAGE));
        } else if (TabResumptionModuleProperties.URL_IMAGE_PROVIDER == propertyKey) {
            moduleView.setUrlImageProvider(
                    model.get(TabResumptionModuleProperties.URL_IMAGE_PROVIDER));
        } else if (TabResumptionModuleProperties.THUMBNAIL_PROVIDER == propertyKey) {
            moduleView.setThumbnailProvider(
                    model.get(TabResumptionModuleProperties.THUMBNAIL_PROVIDER));
        } else if (TabResumptionModuleProperties.SEE_MORE_LINK_CLICK_CALLBACK == propertyKey) {
            moduleView.setSeeMoreLinkClickCallback(
                    model.get(TabResumptionModuleProperties.SEE_MORE_LINK_CLICK_CALLBACK));
        } else if (TabResumptionModuleProperties.CLICK_CALLBACK == propertyKey) {
            moduleView.setClickCallbacks(model.get(TabResumptionModuleProperties.CLICK_CALLBACK));
        } else if (TabResumptionModuleProperties.SUGGESTION_BUNDLE == propertyKey) {
            moduleView.setSuggestionBundle(
                    model.get(TabResumptionModuleProperties.SUGGESTION_BUNDLE));
        } else if (TabResumptionModuleProperties.TITLE == propertyKey) {
            moduleView.setTitle(model.get(TabResumptionModuleProperties.TITLE));
        } else {
            assert false : "Unhandled property detected in TabResumptionModuleViewBinder!";
        }
    }
}
