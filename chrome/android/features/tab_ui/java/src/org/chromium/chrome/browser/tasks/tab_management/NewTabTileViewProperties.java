// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * List of properties used by NewTabTile component.
 */
class NewTabTileViewProperties {
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> ON_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableFloatPropertyKey THUMBNAIL_ASPECT_RATIO =
            new PropertyModel.WritableFloatPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey CARD_HEIGHT_INTERCEPT =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {ON_CLICK_LISTENER,
            THUMBNAIL_ASPECT_RATIO, CARD_HEIGHT_INTERCEPT, IS_INCOGNITO, CARD_TYPE};
}