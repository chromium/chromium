// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the TabSharingToolbar. */
@NullMarked
public class TabSharingToolbarProperties {
    /** The status text to be displayed on the toolbar. */
    public static final WritableObjectPropertyKey<CharSequence> STATUS_TEXT =
            new WritableObjectPropertyKey<>();

    /** The click listener for the stop sharing button. */
    public static final WritableObjectPropertyKey<Runnable> STOP_SHARING_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Whether the "Share this tab instead" button should be visible. */
    public static final WritableBooleanPropertyKey SHARE_INSTEAD_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    /** The click listener for the "Share this tab instead" button. */
    public static final WritableObjectPropertyKey<Runnable> SHARE_INSTEAD_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        STATUS_TEXT,
        STOP_SHARING_CLICK_LISTENER,
        SHARE_INSTEAD_BUTTON_VISIBLE,
        SHARE_INSTEAD_CLICK_LISTENER
    };

    private TabSharingToolbarProperties() {}
}
