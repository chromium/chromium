// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds the custom view for archived tabs. */
public class ArchivedTabsCardViewBinder {
    /**
     * Binder method for the archived tabs custom message
     *
     * @param model The {@link PropertyModel} for the view.
     * @param view The {@link View} to bind.
     * @param key The {@link PropertyKey} to bind.
     */
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        Context context = view.getContext();
        if (ArchivedTabsCardViewProperties.NUMBER_OF_ARCHIVED_TABS == key) {
            int numInactiveTabs = model.get(ArchivedTabsCardViewProperties.NUMBER_OF_ARCHIVED_TABS);
            String title =
                    context.getResources()
                            .getQuantityString(
                                    R.plurals.archived_tab_card_title,
                                    numInactiveTabs,
                                    numInactiveTabs);
            ((TextView) view.findViewById(R.id.title)).setText(title);
        } else if (ArchivedTabsCardViewProperties.ARCHIVE_TIME_DELTA_DAYS == key) {
            int inactiveTimeDeltaDays =
                    model.get(ArchivedTabsCardViewProperties.ARCHIVE_TIME_DELTA_DAYS);
            String subtitle =
                    context.getResources()
                            .getQuantityString(
                                    R.plurals.archived_tab_card_subtitle,
                                    inactiveTimeDeltaDays,
                                    inactiveTimeDeltaDays);
            ((TextView) view.findViewById(R.id.subtitle)).setText(subtitle);
        } else if (ArchivedTabsCardViewProperties.CLICK_HANDLER == key) {
            view.setOnClickListener(
                    v -> {
                        ((Runnable) model.get(ArchivedTabsCardViewProperties.CLICK_HANDLER)).run();
                    });
        } else if (ArchivedTabsCardViewProperties.WIDTH == key) {
            View card = view.findViewById(R.id.card);
            var params = card.getLayoutParams();
            params.width = model.get(ArchivedTabsCardViewProperties.WIDTH);
            card.setLayoutParams(params);
        }
    }
}
