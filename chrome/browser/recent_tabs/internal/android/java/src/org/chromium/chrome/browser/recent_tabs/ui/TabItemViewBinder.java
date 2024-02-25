// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import static org.chromium.chrome.browser.recent_tabs.ui.TabItemProperties.FOREIGN_SESSION_TAB;
import static org.chromium.chrome.browser.recent_tabs.ui.TabItemProperties.IS_SELECTED;
import static org.chromium.chrome.browser.recent_tabs.ui.TabItemProperties.ON_CLICK_LISTENER;

import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.R;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** A binder class for review tabs items on the detail sheet. */
public class TabItemViewBinder {
    /** A class to hold objects used for tab favicon fetching. */
    static class BindContext {
        private final DefaultFaviconHelper mDefaultFaviconHelper;
        private final RoundedIconGenerator mIconGenerator;
        private final Profile mProfile;
        private @Nullable FaviconHelper mFaviconHelper;

        BindContext(
                DefaultFaviconHelper defaultFaviconHelper,
                RoundedIconGenerator iconGenerator,
                FaviconHelper faviconHelper,
                Profile profile) {
            mDefaultFaviconHelper = defaultFaviconHelper;
            mIconGenerator = iconGenerator;
            mFaviconHelper = faviconHelper;
            mProfile = profile;
        }

        public DefaultFaviconHelper getDefaultFaviconHelper() {
            return mDefaultFaviconHelper;
        }

        public RoundedIconGenerator getIconGenerator() {
            return mIconGenerator;
        }

        public @Nullable FaviconHelper getFaviconHelper() {
            return mFaviconHelper;
        }

        public Profile getProfile() {
            return mProfile;
        }

        public void destroy() {
            // If the FaviconHelper is still non-null before destroy, remove it.
            if (mFaviconHelper != null) {
                mFaviconHelper = null;
            }
        }
    }

    static View create(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.restore_tabs_tab_item, parent, false);
    }

    static void bind(
            PropertyModel model, View view, PropertyKey propertyKey, BindContext bindContext) {
        if (propertyKey == FOREIGN_SESSION_TAB) {
            ForeignSessionTab tab = model.get(FOREIGN_SESSION_TAB);
            assert tab.url != null;

            TextView tabNameView = view.findViewById(R.id.restore_tabs_detail_sheet_tab_name);
            tabNameView.setText(tab.title);

            TextView tabInfoView = view.findViewById(R.id.restore_tabs_detail_sheet_tab_info);
            tabInfoView.setText(tab.url.getSpec());

            // Ensure that the previous state is saved if the user navigates from the promo screen
            // back to the review tabs screen without changing devices.
            CheckBox checkBoxView = view.findViewById(R.id.restore_tabs_tab_item_checkbox);
            checkBoxView.setChecked(model.get(IS_SELECTED));
            int faviconSize =
                    view.getContext()
                            .getResources()
                            .getDimensionPixelSize(R.dimen.default_favicon_size);

            // Load favicon
            ImageView faviconView = view.findViewById(R.id.restore_tabs_review_tabs_screen_favicon);
            FaviconImageCallback imageCallback =
                    new FaviconImageCallback() {
                        @Override
                        public void onFaviconAvailable(Bitmap bitmap, GURL iconUrl) {
                            if (bitmap != null) {
                                Drawable faviconDrawable =
                                        FaviconUtils.getIconDrawableWithFilter(
                                                bitmap,
                                                tab.url,
                                                bindContext.getIconGenerator(),
                                                bindContext.getDefaultFaviconHelper(),
                                                view.getContext(),
                                                faviconSize);
                                faviconView.setImageDrawable(faviconDrawable);
                            }
                        }
                    };

            FaviconHelper faviconHelper = bindContext.getFaviconHelper();
            // If the faviconHelper is null, possibly when the feature was exited and invoked
            // destroy methods before the favicon was attempted to be fetched, do not fetch the
            // favicon to avoid a native crash.
            if (faviconHelper != null) {
                faviconHelper.getForeignFaviconImageForURL(
                        bindContext.getProfile(), tab.url, faviconSize, imageCallback);
            }

            Drawable image =
                    bindContext
                            .getDefaultFaviconHelper()
                            .getDefaultFaviconDrawable(view.getContext(), tab.url, true);
            faviconView.setImageDrawable(image);
        } else if (propertyKey == ON_CLICK_LISTENER) {
            view.setOnClickListener((v) -> model.get(ON_CLICK_LISTENER).run());
        } else if (propertyKey == IS_SELECTED) {
            CheckBox checkBoxView = view.findViewById(R.id.restore_tabs_tab_item_checkbox);
            checkBoxView.setChecked(model.get(IS_SELECTED));
        }
    }
}
