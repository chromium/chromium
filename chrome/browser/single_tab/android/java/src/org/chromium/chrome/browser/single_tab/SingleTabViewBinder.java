// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.single_tab;

import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.LATERAL_MARGIN;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.SEE_MORE_LINK_CLICK_LISTENER;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.TAB_THUMBNAIL;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.TITLE;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.URL;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

// The view binder of the single tab view.
public class SingleTabViewBinder {
    public static void bind(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        if (propertyKey == CLICK_LISTENER) {
            view.setOnClickListener(model.get(CLICK_LISTENER));
        } else if (propertyKey == SEE_MORE_LINK_CLICK_LISTENER) {
            ((SingleTabView) view)
                    .setOnSeeMoreLinkClickListener(model.get(SEE_MORE_LINK_CLICK_LISTENER));
        } else if (propertyKey == FAVICON) {
            ((SingleTabView) view).setFavicon(model.get(FAVICON));
        } else if (propertyKey == TAB_THUMBNAIL) {
            ((SingleTabView) view).setTabThumbnail(model.get(TAB_THUMBNAIL));
        } else if (propertyKey == IS_VISIBLE) {
            view.setVisibility(model.get(IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == TITLE) {
            ((SingleTabView) view).setTitle(model.get(TITLE));
        } else if (propertyKey == URL) {
            ((SingleTabView) view).setUrl(model.get(URL));
        } else if (propertyKey == LATERAL_MARGIN) {
            MarginLayoutParams marginLayoutParams = (MarginLayoutParams) view.getLayoutParams();
            int lateralMargin = model.get(LATERAL_MARGIN);
            marginLayoutParams.setMarginStart(lateralMargin);
            marginLayoutParams.setMarginEnd(lateralMargin);
            view.setLayoutParams(marginLayoutParams);
        } else {
            assert false : "Unsupported property key";
        }
    }
}
