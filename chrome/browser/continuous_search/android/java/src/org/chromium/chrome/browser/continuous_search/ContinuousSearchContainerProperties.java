// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains model properties for the container UI of Continuous Search Navigation.
 */
class ContinuousSearchContainerProperties {
    static final PropertyModel.WritableIntPropertyKey ANDROID_VIEW_VISIBILITY =
            new PropertyModel.WritableIntPropertyKey();
    static final PropertyModel.WritableBooleanPropertyKey COMPOSITED_VIEW_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    /**
     * Current vertical offset of the view. This is used for both the Java view and the composited
     * view.
     */
    static final PropertyModel.WritableIntPropertyKey VERTICAL_OFFSET =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
            ANDROID_VIEW_VISIBILITY, COMPOSITED_VIEW_VISIBLE, VERTICAL_OFFSET};
}
