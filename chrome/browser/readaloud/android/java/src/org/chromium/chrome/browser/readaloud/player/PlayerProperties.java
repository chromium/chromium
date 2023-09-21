// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Keys for Read Aloud player model properties. */
class PlayerProperties {
    public static final WritableObjectPropertyKey<InteractionHandler> INTERACTION_HANDLER =
            new WritableObjectPropertyKey<>();
    public static final PropertyKey[] ALL_KEYS = {
            INTERACTION_HANDLER //
    };
}
