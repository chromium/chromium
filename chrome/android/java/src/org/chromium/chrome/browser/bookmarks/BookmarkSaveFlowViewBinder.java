// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.CompoundButton;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/** ViewBinder for the bookmarks save flow. */
public class BookmarkSaveFlowViewBinder implements ViewBinder<PropertyModel, View, PropertyKey> {
    @Override
    public void bind(PropertyModel model, View view, PropertyKey propertyKey) {
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
        } else if (propertyKey == BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_START_ICON_RES) {
            ((ImageView) view.findViewById(R.id.notification_switch_start_icon))
                    .setImageDrawable(AppCompatResources.getDrawable(view.getContext(),
                            model.get(BookmarkSaveFlowProperties
                                              .NOTIFICATION_SWITCH_START_ICON_RES)));
        } else if (propertyKey == BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_SUBTITLE) {
            ((TextView) view.findViewById(R.id.notification_switch_subtitle))
                    .setText(model.get(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_SUBTITLE));
        } else if (propertyKey == BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TITLE) {
            ((TextView) view.findViewById(R.id.notification_switch_title))
                    .setText(model.get(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TITLE));
        } else if (propertyKey == BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED) {
            ((CompoundButton) view.findViewById(R.id.notification_switch))
                    .setChecked(model.get(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED));
        } else if (propertyKey == BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLE_LISTENER) {
            ((CompoundButton) view.findViewById(R.id.notification_switch))
                    .setOnCheckedChangeListener(model.get(
                            BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLE_LISTENER));
        } else if (propertyKey == BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_VISIBLE) {
            view.findViewById(R.id.notification_switch_divider)
                    .setVisibility(model.get(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_VISIBLE)
                                    ? View.VISIBLE
                                    : View.GONE);
            view.findViewById(R.id.notification_switch_container)
                    .setVisibility(model.get(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_VISIBLE)
                                    ? View.VISIBLE
                                    : View.GONE);
        } else if (propertyKey == BookmarkSaveFlowProperties.NOTIFICATION_UI_ENABLED) {
            boolean enabled = model.get(BookmarkSaveFlowProperties.NOTIFICATION_UI_ENABLED);
            Drawable drawable = ((ImageView) view.findViewById(R.id.notification_switch_start_icon))
                                        .getDrawable();
            if (drawable != null) {
                final @ColorInt int color = enabled
                        ? SemanticColorUtils.getDefaultIconColor(view.getContext())
                        : ContextCompat.getColor(
                                view.getContext(), R.color.default_icon_color_disabled);

                drawable.setColorFilter(color, PorterDuff.Mode.SRC_IN);
            }

            ApiCompatibilityUtils.setTextAppearance(
                    view.findViewById(R.id.notification_switch_title),
                    enabled ? R.style.TextAppearance_TextMedium_Primary
                            : R.style.TextAppearance_TextMedium_Disabled);
        } else if (propertyKey == BookmarkSaveFlowProperties.SUBTITLE_TEXT) {
            ((TextView) view.findViewById(R.id.subtitle_text))
                    .setText(model.get(BookmarkSaveFlowProperties.SUBTITLE_TEXT));
        } else if (propertyKey == BookmarkSaveFlowProperties.TITLE_TEXT) {
            ((TextView) view.findViewById(R.id.title_text))
                    .setText(model.get(BookmarkSaveFlowProperties.TITLE_TEXT));
        }
    }
}
