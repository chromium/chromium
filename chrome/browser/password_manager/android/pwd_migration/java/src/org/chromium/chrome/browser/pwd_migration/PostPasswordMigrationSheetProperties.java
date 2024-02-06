// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties defined here reflect the visible state of the post password migration sheet */
class PostPasswordMigrationSheetProperties {
    static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("visible");
    static final PropertyModel.ReadableObjectPropertyKey<Callback<Integer>> DISMISS_HANDLER =
            new PropertyModel.ReadableObjectPropertyKey<>("dismiss_handler");

    static PropertyModel createDefaultModel(Callback<Integer> dismissHandler) {
        return new PropertyModel.Builder(VISIBLE, DISMISS_HANDLER)
                .with(VISIBLE, false)
                .with(DISMISS_HANDLER, dismissHandler)
                .build();
    }

    private PostPasswordMigrationSheetProperties() {}
}
