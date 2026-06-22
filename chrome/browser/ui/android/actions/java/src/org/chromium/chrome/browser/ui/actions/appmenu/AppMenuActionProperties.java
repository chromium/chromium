// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.appmenu;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the app menu action button. */
@NullMarked
public class AppMenuActionProperties {
    public static final WritableBooleanPropertyKey SHOW_UPDATE_BADGE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<MenuButtonState> UPDATE_BADGE_BUTTON_STATE =
            new WritableObjectPropertyKey<>();

    /** All properties for the app menu action button. */
    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(
                    ActionProperties.BASE_KEYS,
                    new PropertyKey[] {SHOW_UPDATE_BADGE, UPDATE_BADGE_BUTTON_STATE});
}
