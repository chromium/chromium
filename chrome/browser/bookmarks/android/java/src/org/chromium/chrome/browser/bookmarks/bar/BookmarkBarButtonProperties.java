// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.graphics.drawable.Drawable;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.util.ClickWithMetaStateCallback;

/**
 * Properties for a button in the bookmark bar which provides users with bookmark access from top
 * chrome.
 */
@NullMarked
class BookmarkBarButtonProperties {

    /**
     * The callback to notify of bookmark bar button click events. The callback is provided the meta
     * state of the most recent key/touch event.
     */
    public static final WritableObjectPropertyKey<ClickWithMetaStateCallback> CLICK_CALLBACK =
            new WritableObjectPropertyKey<>();

    /** The supplier for the icon to render in the bookmark bar button. */
    public static final WritableObjectPropertyKey<LazyOneshotSupplier<Drawable>> ICON_SUPPLIER =
            new WritableObjectPropertyKey<>();

    /**
     * The resource identifier for the tint list of the icon to render in the bookmark bar button.
     * Note that this property may be set to {@link Resources.ID_NULL} to clear the tint list.
     */
    public static final WritableIntPropertyKey ICON_TINT_LIST_ID = new WritableIntPropertyKey();

    /** The title to render in the bookmark bar button. */
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {CLICK_CALLBACK, ICON_SUPPLIER, ICON_TINT_LIST_ID, TITLE};
}
