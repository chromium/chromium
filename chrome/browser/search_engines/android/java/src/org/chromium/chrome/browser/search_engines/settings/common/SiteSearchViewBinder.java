// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.common;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class SiteSearchViewBinder {
    // TODO: Add unit tests for this class.

    public static class ViewHolder {
        final TextView mTitle;
        final TextView mShortcut;
        final ImageView mIcon;
        final TextView mText;
        final ImageView mActionIcon;

        public ViewHolder(View view) {
            mTitle = view.findViewById(R.id.name);
            mShortcut = view.findViewById(R.id.shortcut);
            mIcon = view.findViewById(R.id.favicon);
            mText = view.findViewById(R.id.text);
            mActionIcon = view.findViewById(R.id.action_icon);
        }
    }

    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        ViewHolder holder = (ViewHolder) view.getTag();
        if (SiteSearchProperties.SITE_NAME == propertyKey) {
            holder.mTitle.setText(model.get(SiteSearchProperties.SITE_NAME));
        } else if (SiteSearchProperties.SITE_SHORTCUT == propertyKey) {
            holder.mShortcut.setText(model.get(SiteSearchProperties.SITE_SHORTCUT));
        } else if (SiteSearchProperties.ICON == propertyKey) {
            holder.mIcon.setImageBitmap(model.get(SiteSearchProperties.ICON));
        } else if (SiteSearchProperties.ON_CLICK == propertyKey) {
            view.setOnClickListener(model.get(SiteSearchProperties.ON_CLICK));
        } else if (SiteSearchProperties.TEXT == propertyKey) {
            holder.mText.setText(model.get(SiteSearchProperties.TEXT));
        } else if (SiteSearchProperties.IS_EXPANDED == propertyKey) {
            boolean isExpanded = model.get(SiteSearchProperties.IS_EXPANDED);
            int iconRes =
                    isExpanded
                            ? R.drawable.ic_expand_less_black_24dp
                            : R.drawable.ic_expand_more_black_24dp;
            holder.mActionIcon.setImageResource(iconRes);
        } else if (SiteSearchProperties.MENU_DELEGATE == propertyKey) {
            ListMenuButton button = (ListMenuButton) view.findViewById(R.id.overflow_menu_button);
            ListMenuDelegate delegate = model.get(SiteSearchProperties.MENU_DELEGATE);
            button.setDelegate(delegate);
            button.setEnabled(delegate != null);
        }
    }

    public static void bindPreference(
            PropertyModel model, SearchEngineListPreference pref, PropertyKey propertyKey) {
        if (SiteSearchProperties.ADAPTER == propertyKey) {
            pref.setAdapter(model.get(SiteSearchProperties.ADAPTER));
        }
    }
}
