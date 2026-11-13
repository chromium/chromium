// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.glic;

import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties specific to the Glic action. */
@NullMarked
public class GlicActionProperties {
    public static final WritableObjectPropertyKey<Drawable> GLIC_DRAWABLE =
            new WritableObjectPropertyKey<>(true);

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ActionProperties.ALL_KEYS, new PropertyKey[] {GLIC_DRAWABLE});
}
