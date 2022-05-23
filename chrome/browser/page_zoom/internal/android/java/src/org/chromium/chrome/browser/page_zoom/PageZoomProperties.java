// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_zoom;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Properties for the page zoom feature.
 */
class PageZoomProperties {
    static final WritableIntPropertyKey MAXIMUM_ZOOM = new WritableIntPropertyKey();
    static final WritableIntPropertyKey CURRENT_ZOOM = new WritableIntPropertyKey();
    static final WritableObjectPropertyKey<Callback<Void>> DECREASE_ZOOM_CALLBACK =
            new WritableObjectPropertyKey<Callback<Void>>();
    static final WritableObjectPropertyKey<Callback<Void>> INCREASE_ZOOM_CALLBACK =
            new WritableObjectPropertyKey<Callback<Void>>();
    static final PropertyKey[] ALL_KEYS = {
            MAXIMUM_ZOOM, CURRENT_ZOOM, DECREASE_ZOOM_CALLBACK, INCREASE_ZOOM_CALLBACK};
}
