// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
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

    /**
     * The callback to notify of keyboard events on the button. Used to handle key presses like
     * Enter for navigation and actions.
     */
    public static final WritableObjectPropertyKey<View.OnKeyListener> KEY_LISTENER =
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

    /** The tooltip to display on hover for the bookmark bar button. */
    public static final WritableObjectPropertyKey<String> TOOLTIP =
            new WritableObjectPropertyKey<>();

    /** The content description for folders in the bookmark bar. */
    public static final WritableObjectPropertyKey<String> FOLDER_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** The style resource identifier for the title's text appearance. */
    public static final WritableIntPropertyKey TEXT_APPEARANCE_ID = new WritableIntPropertyKey();

    /** The {@link BookmarkItem} associated with this button. */
    public static final ReadableObjectPropertyKey<BookmarkItem> BOOKMARK_ITEM =
            new ReadableObjectPropertyKey<>();

    /** The background drawable for this button. */
    public static final WritableIntPropertyKey BACKGROUND_DRAWABLE_ID =
            new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                CLICK_CALLBACK,
                KEY_LISTENER,
                ICON_SUPPLIER,
                ICON_TINT_LIST_ID,
                TITLE,
                TOOLTIP,
                FOLDER_CONTENT_DESCRIPTION,
                TEXT_APPEARANCE_ID,
                BOOKMARK_ITEM,
                BACKGROUND_DRAWABLE_ID
            };
}
