// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.media;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Items for the list view in the media capture picker. */
public class MediaCapturePickerItemProperties {
    /** Listener to be called when a tab is selected. */
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    /** Name of the tab. */
    public static final PropertyModel.WritableObjectPropertyKey<String> TAB_NAME =
            new PropertyModel.WritableObjectPropertyKey<>();

    /** Whether this tab is currently selected or not. */
    public static final PropertyModel.WritableBooleanPropertyKey SELECTED =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {CLICK_LISTENER, TAB_NAME, SELECTED};
}
