// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.expandedplayer;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Keys for Read Aloud expanded player model properties. */
public class ExpandedPlayerProperties {
    public static final WritableObjectPropertyKey<Integer> STATE_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Float> SPEED_KEY =
            new WritableObjectPropertyKey<>();
    public static final PropertyKey[] ALL_KEYS = {STATE_KEY, SPEED_KEY};
}
