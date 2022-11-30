// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class HistoryClustersToolbarProperties {
    static final WritableObjectPropertyKey<QueryState> QUERY_STATE =
            new WritableObjectPropertyKey<>("query state");

    static final PropertyKey[] ALL_KEYS = {QUERY_STATE};
}
