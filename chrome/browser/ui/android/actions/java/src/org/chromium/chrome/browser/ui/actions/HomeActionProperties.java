// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.util.ClickWithMetaStateCallback;

/** Properties specific to the Home Button action. */
@NullMarked
public class HomeActionProperties {
    public static final WritableObjectPropertyKey<ListMenuDelegate> LONG_PRESS_MENU_DELEGATE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<ClickWithMetaStateCallback>
            CLICK_WITH_META_CALLBACK = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(
                    ActionProperties.BASE_KEYS,
                    new PropertyKey[] {
                        LONG_PRESS_MENU_DELEGATE, CLICK_WITH_META_CALLBACK,
                    });
}
