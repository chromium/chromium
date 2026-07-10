// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings.personal_context;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Collection of properties that affect the Autofill Personal Context settings screen. */
@NullMarked
class AutofillPersonalContextProperties {
    static final WritableBooleanPropertyKey PERSONAL_CONTEXT_ENABLED =
            new WritableBooleanPropertyKey("personal_context_enabled");
    static final ReadableObjectPropertyKey<Callback<Boolean>> ON_PERSONAL_CONTEXT_TOGGLE_CHANGED =
            new ReadableObjectPropertyKey<>("on_personal_context_toggle_changed");
    static final ReadableObjectPropertyKey<Runnable> ON_MANAGE_CONNECTED_APPS_CLICKED =
            new ReadableObjectPropertyKey<>("on_manage_connected_apps_clicked");

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                PERSONAL_CONTEXT_ENABLED,
                ON_PERSONAL_CONTEXT_TOGGLE_CHANGED,
                ON_MANAGE_CONNECTED_APPS_CLICKED,
            };

    private AutofillPersonalContextProperties() {}
}
