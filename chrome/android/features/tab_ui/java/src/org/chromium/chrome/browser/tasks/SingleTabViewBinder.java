// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.browser.tasks.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.tasks.SingleTabViewProperties.TITLE;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

// The view binder of the single tab view.
class SingleTabViewBinder {
    public static void bind(PropertyModel model, SingleTabView view, PropertyKey propertyKey) {
        if (propertyKey == CLICK_LISTENER) {
            view.setOnClickListener(model.get(CLICK_LISTENER));
        } else if (propertyKey == FAVICON) {
            view.setFavicon(model.get(FAVICON));
        } else if (propertyKey == IS_VISIBLE) {
            view.setVisibility(model.get(IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == TITLE) {
            view.setTitle(model.get(TITLE));
        } else {
            assert false : "Unsupported property key";
        }
    }
}