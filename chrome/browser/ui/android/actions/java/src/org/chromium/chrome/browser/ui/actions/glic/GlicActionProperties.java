// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.glic;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Properties specific to the Glic action. */
@NullMarked
public class GlicActionProperties {
    @IntDef({GlicState.DEFAULT, GlicState.WORKING, GlicState.NEEDS_REVIEW, GlicState.DONE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface GlicState {
        int DEFAULT = 0;
        int WORKING = 1;
        int NEEDS_REVIEW = 2;
        int DONE = 3;
    }

    public static final WritableIntPropertyKey GLIC_STATE = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ActionProperties.ALL_KEYS, new PropertyKey[] {GLIC_STATE});
}
