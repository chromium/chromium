// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** View Properties related to displaying a most visited list. */
public final class MostVisitedListProperties {
    private MostVisitedListProperties() {}

    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey INTERVAL_PADDINGS =
            new WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey EDGE_PADDINGS =
            new WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey LEFT_RIGHT_MARGINS =
            new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {IS_VISIBLE, INTERVAL_PADDINGS, EDGE_PADDINGS, LEFT_RIGHT_MARGINS};
}
