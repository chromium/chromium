// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class HistoryClustersBottomSheetToolbarProperties {
    static final WritableObjectPropertyKey<OnClickListener> OPEN_ACTIVITY_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>("open in new window click listener");
    static final WritableObjectPropertyKey<String> QUERY_TEXT =
            new WritableObjectPropertyKey<>("query text");

    static final PropertyKey[] ALL_KEYS = {OPEN_ACTIVITY_BUTTON_CLICK_LISTENER, QUERY_TEXT};
}
