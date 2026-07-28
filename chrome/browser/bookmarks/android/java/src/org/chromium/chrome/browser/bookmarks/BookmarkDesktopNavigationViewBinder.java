// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the bookmark desktop navigation pane. */
@NullMarked
class BookmarkDesktopNavigationViewBinder {
    /** Binds the folder item. */
    public static void bindFolder(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == BookmarkDesktopNavigationProperties.TITLE) {
            TextView title = view.findViewById(R.id.title);
            title.setText(model.get(BookmarkDesktopNavigationProperties.TITLE));
        } else if (propertyKey == BookmarkDesktopNavigationProperties.ICON) {
            ImageView icon = view.findViewById(R.id.icon);
            icon.setImageDrawable(model.get(BookmarkDesktopNavigationProperties.ICON));
        } else if (propertyKey == BookmarkDesktopNavigationProperties.IS_SELECTED) {
            view.setSelected(model.get(BookmarkDesktopNavigationProperties.IS_SELECTED));
        } else if (propertyKey == BookmarkDesktopNavigationProperties.ON_CLICK_HANDLER) {
            view.setOnClickListener(
                    v -> {
                        Runnable handler =
                                model.get(BookmarkDesktopNavigationProperties.ON_CLICK_HANDLER);
                        if (handler != null) {
                            handler.run();
                        }
                    });
        }
    }

    /** Binds the header item. */
    public static void bindHeader(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == BookmarkDesktopNavigationProperties.HEADER_TITLE) {
            TextView title = view.findViewById(R.id.title);
            title.setText(model.get(BookmarkDesktopNavigationProperties.HEADER_TITLE));
        }
    }
}
