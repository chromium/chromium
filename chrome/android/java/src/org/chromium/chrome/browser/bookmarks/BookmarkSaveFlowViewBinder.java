// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

/** ViewBinder for the bookmarks save flow. */
public class BookmarkSaveFlowViewBinder
        implements ViewBinder<PropertyModel, ViewLookupCachingFrameLayout, PropertyKey> {
    @Override
    public void bind(
            PropertyModel model, ViewLookupCachingFrameLayout view, PropertyKey propertyKey) {
        if (propertyKey == BookmarkSaveFlowProperties.EDIT_ONCLICK_LISTENER) {
            view.findViewById(R.id.bookmark_edit)
                    .setOnClickListener(
                            model.get(BookmarkSaveFlowProperties.EDIT_ONCLICK_LISTENER));
        } else if (propertyKey == BookmarkSaveFlowProperties.FOLDER_SELECT_ICON) {
            ((ImageView) view.findViewById(R.id.bookmark_select_folder))
                    .setImageDrawable(model.get(BookmarkSaveFlowProperties.FOLDER_SELECT_ICON));
        } else if (propertyKey == BookmarkSaveFlowProperties.FOLDER_SELECT_ICON_ENABLED) {
            ((ImageView) view.findViewById(R.id.bookmark_select_folder))
                    .setEnabled(model.get(BookmarkSaveFlowProperties.FOLDER_SELECT_ICON_ENABLED));
        } else if (propertyKey == BookmarkSaveFlowProperties.FOLDER_SELECT_ONCLICK_LISTENER) {
            view.findViewById(R.id.bookmark_select_folder)
                    .setOnClickListener(
                            model.get(BookmarkSaveFlowProperties.FOLDER_SELECT_ONCLICK_LISTENER));
        } else if (propertyKey == BookmarkSaveFlowProperties.SUBTITLE_TEXT) {
            ((TextView) view.findViewById(R.id.subtitle_text))
                    .setText(model.get(BookmarkSaveFlowProperties.SUBTITLE_TEXT));
        } else if (propertyKey == BookmarkSaveFlowProperties.TITLE_TEXT) {
            ((TextView) view.findViewById(R.id.title_text))
                    .setText(model.get(BookmarkSaveFlowProperties.TITLE_TEXT));
        }
    }
}
