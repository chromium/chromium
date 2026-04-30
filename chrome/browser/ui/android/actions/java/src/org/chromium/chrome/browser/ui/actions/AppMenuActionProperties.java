// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties specific to the App Menu action. */
@NullMarked
public class AppMenuActionProperties {
    public static final WritableObjectPropertyKey<Callback<View>> APP_MENU_SETUP_CALLBACK =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(
                    ActionProperties.BASE_KEYS,
                    new PropertyKey[] {
                        APP_MENU_SETUP_CALLBACK,
                    });
}
