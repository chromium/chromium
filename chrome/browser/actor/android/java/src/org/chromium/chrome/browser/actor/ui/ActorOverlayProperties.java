// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Properties defined for the Actor Overlay model. */
@NullMarked
class ActorOverlayProperties {
    /** Whether the overlay is currently visible. */
    public static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    /** All keys for the property model. */
    public static final PropertyKey[] ALL_KEYS = {VISIBLE};
}
