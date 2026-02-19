// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_site_search;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class CustomSiteSearchViewBinder {
    static class ViewHolder {
        final TextView mTitle;
        final TextView mUrl;
        final TextView mText;
        final ImageView mActionIcon;

        public ViewHolder(View view) {
            mTitle = view.findViewById(R.id.name);
            mUrl = view.findViewById(R.id.url);
            mText = view.findViewById(R.id.text);
            mActionIcon = view.findViewById(R.id.action_icon);
        }
    }

    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        ViewHolder holder = (ViewHolder) view.getTag();
        if (CustomSiteSearchProperties.SITE_NAME == propertyKey) {
            holder.mTitle.setText(model.get(CustomSiteSearchProperties.SITE_NAME));
        } else if (CustomSiteSearchProperties.SITE_URL == propertyKey) {
            holder.mUrl.setText(model.get(CustomSiteSearchProperties.SITE_URL));
        } else if (CustomSiteSearchProperties.ON_CLICK == propertyKey) {
            view.setOnClickListener(v -> model.get(CustomSiteSearchProperties.ON_CLICK).run());
        } else if (CustomSiteSearchProperties.TEXT == propertyKey) {
            holder.mText.setText(model.get(CustomSiteSearchProperties.TEXT));
        } else if (CustomSiteSearchProperties.IS_EXPANDED == propertyKey) {
            boolean isExpanded = model.get(CustomSiteSearchProperties.IS_EXPANDED);
            int iconRes =
                    isExpanded
                            ? R.drawable.ic_expand_less_black_24dp
                            : R.drawable.ic_expand_more_black_24dp;
            holder.mActionIcon.setImageResource(iconRes);
        }
    }

    public static void bindPreference(
            PropertyModel model, CustomSiteSearchListPreference pref, PropertyKey propertyKey) {
        if (CustomSiteSearchProperties.ADAPTER == propertyKey) {
            pref.setAdapter(model.get(CustomSiteSearchProperties.ADAPTER));
        }
    }
}
