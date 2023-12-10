// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import static org.chromium.chrome.browser.recent_tabs.ui.ForeignSessionItemProperties.IS_SELECTED;
import static org.chromium.chrome.browser.recent_tabs.ui.ForeignSessionItemProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.recent_tabs.ui.ForeignSessionItemProperties.SESSION_PROFILE;

import android.text.format.DateUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.recent_tabs.R;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A binder class for device items on the detail sheet. */
public class ForeignSessionItemViewBinder {
    static View create(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.restore_tabs_foreign_session_item, parent, false);
    }

    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == SESSION_PROFILE) {
            ForeignSession session = model.get(SESSION_PROFILE);
            TextView sessionNameView =
                    view.findViewById(R.id.restore_tabs_detail_sheet_device_name);
            sessionNameView.setText(session.name);

            ImageView deviceIconView =
                    view.findViewById(R.id.restore_tabs_device_sheet_device_icon);
            if (session.formFactor == FormFactor.PHONE) {
                deviceIconView.setImageResource(R.drawable.restore_tabs_phone_icon);
            } else if (session.formFactor == FormFactor.TABLET) {
                deviceIconView.setImageResource(R.drawable.restore_tabs_tablet_icon);
            }
            assert (session.formFactor == FormFactor.PHONE
                            || session.formFactor == FormFactor.TABLET)
                    : "Unsupported form factor device retrieved.";

            String sessionInfo = getSessionInfo(view, session);
            TextView sessionInfoView =
                    view.findViewById(R.id.restore_tabs_detail_sheet_session_info);
            sessionInfoView.setText(sessionInfo);
        } else if (propertyKey == ON_CLICK_LISTENER) {
            view.setOnClickListener((v) -> model.get(ON_CLICK_LISTENER).run());
        } else if (propertyKey == IS_SELECTED) {
            view.findViewById(R.id.restore_tabs_detail_sheet_device_item_selected_icon)
                    .setVisibility(model.get(IS_SELECTED) ? View.VISIBLE : View.GONE);
        }
        setAccessibilityContent(view, model.get(SESSION_PROFILE), model.get(IS_SELECTED));
    }

    private static void setAccessibilityContent(
            View view, ForeignSession session, boolean isSelected) {
        String sessionInfo = getSessionInfo(view, session);
        StringBuilder builder = new StringBuilder();
        builder.append(session.name);
        builder.append(sessionInfo);
        builder.append(
                view.getContext()
                        .getResources()
                        .getString(
                                isSelected
                                        ? R.string.restore_tabs_device_screen_selected_description
                                        : R.string
                                                .restore_tabs_device_screen_not_selected_description));
        view.setContentDescription(builder.toString());
    }

    private static String getSessionInfo(View view, ForeignSession session) {
        int tabCount = 0;
        for (ForeignSessionWindow window : session.windows) {
            tabCount += window.tabs.size();
        }

        CharSequence lastModifiedTimeString =
                DateUtils.getRelativeTimeSpanString(
                        session.modifiedTime, System.currentTimeMillis(), 0);
        String sessionInfo =
                view.getContext()
                        .getResources()
                        .getQuantityString(
                                R.plurals.restore_tabs_promo_sheet_device_info,
                                tabCount,
                                Integer.toString(tabCount),
                                lastModifiedTimeString);
        return sessionInfo;
    }
}
