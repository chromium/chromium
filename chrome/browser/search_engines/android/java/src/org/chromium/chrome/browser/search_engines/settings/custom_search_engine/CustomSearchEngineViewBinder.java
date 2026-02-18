// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_search_engine;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class CustomSearchEngineViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (CustomSearchEngineProperties.NAME == propertyKey) {
            ((TextView) view.findViewById(R.id.name))
                    .setText(model.get(CustomSearchEngineProperties.NAME));
        } else if (CustomSearchEngineProperties.URL == propertyKey) {
            ((TextView) view.findViewById(R.id.url))
                    .setText(model.get(CustomSearchEngineProperties.URL));
        } else if (CustomSearchEngineProperties.ICON == propertyKey) {
            ((ImageView) view.findViewById(R.id.favicon))
                    .setImageBitmap(model.get(CustomSearchEngineProperties.ICON));
        } else if (CustomSearchEngineProperties.CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(CustomSearchEngineProperties.CLICK_LISTENER));
        } else if (CustomSearchEngineProperties.MENU_CLICK_LISTENER == propertyKey) {
            view.findViewById(R.id.overflow_menu_button)
                    .setOnClickListener(
                            model.get(CustomSearchEngineProperties.MENU_CLICK_LISTENER));
        }
    }

    public static void bindPreference(
            PropertyModel model, CustomSearchEngineListPreference pref, PropertyKey propertyKey) {
        if (CustomSearchEngineProperties.ADAPTER == propertyKey) {
            pref.setAdapter(model.get(CustomSearchEngineProperties.ADAPTER));
        }
    }
}
