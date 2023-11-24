// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties associated with rendering an item in the share sheet. */
final class ShareSheetItemViewProperties {
    public static final WritableObjectPropertyKey<Drawable> ICON =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> LABEL = new WritableObjectPropertyKey();

    public static final WritableObjectPropertyKey<String> CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey();

    public static final WritableObjectPropertyKey<OnClickListener> CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey SHOW_NEW_BADGE =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        ICON, LABEL, CONTENT_DESCRIPTION, CLICK_LISTENER, SHOW_NEW_BADGE
    };
}
