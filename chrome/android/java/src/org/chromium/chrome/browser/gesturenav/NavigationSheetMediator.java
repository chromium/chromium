// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import static org.chromium.chrome.browser.gesturenav.NavigationSheetCoordinator.NAVIGATION_LIST_ITEM_TYPE_ID;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.favicon.FaviconUtils;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.widget.RoundedIconGenerator;
import org.chromium.chrome.browser.ui.widget.TintedDrawable;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Mediator class for navigation sheet.
 */
class NavigationSheetMediator {
    private final ClickListener mClickListener;
    private final FaviconHelper mFaviconHelper;
    private final RoundedIconGenerator mIconGenerator;
    private final int mFaviconSize;
    private final ModelList mModelList;
    private final Drawable mHistoryIcon;
    private final Drawable mDefaultIcon;
    private final String mNewTabText;

    private NavigationHistory mHistory;

    /**
     * Performs an action when a navigation item is clicked.
     */
    interface ClickListener {
        /**
         * @param index Index from {@link NavigationEntry#getIndex()}.
         * @param position Position of the clicked item in the list, starting from 0.
         */
        void click(int position, int index);
    }

    static class ItemProperties {
        /** The favicon for the list item. */
        public static final WritableObjectPropertyKey<Drawable> ICON =
                new WritableObjectPropertyKey<>();

        /** The text shown next to the favicon. */
        public static final WritableObjectPropertyKey<String> LABEL =
                new WritableObjectPropertyKey<>();

        /** {@link View#OnClickListener} to execute when each item is clicked. */
        public static final WritableObjectPropertyKey<View.OnClickListener> CLICK_LISTENER =
                new WritableObjectPropertyKey<>();

        public static final PropertyKey[] ALL_KEYS = {ICON, LABEL, CLICK_LISTENER};
    }

    NavigationSheetMediator(Context context, ModelList modelList, ClickListener listener) {
        mModelList = modelList;
        mClickListener = listener;
        mFaviconHelper = new FaviconHelper();
        mIconGenerator = FaviconUtils.createCircularIconGenerator(context.getResources());
        mFaviconSize = context.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        mHistoryIcon = TintedDrawable.constructTintedDrawable(
                context, R.drawable.ic_history_googblue_24dp, R.color.default_icon_color);
        mDefaultIcon = TintedDrawable.constructTintedDrawable(
                context, R.drawable.ic_chrome, R.color.default_icon_color);
        mNewTabText = context.getResources().getString(R.string.menu_new_tab);
    }

    /**
     * Populate the sheet with the navigation history.
     * @param history {@link NavigationHistory} object.
     */
    void populateEntries(NavigationHistory history) {
        mHistory = history;
        Set<String> requestedUrls = new HashSet<String>();
        for (int i = 0; i < mHistory.getEntryCount(); i++) {
            PropertyModel model = new PropertyModel(Arrays.asList(ItemProperties.ALL_KEYS));
            NavigationEntry entry = mHistory.getEntryAtIndex(i);
            model.set(ItemProperties.LABEL, getEntryText(entry));
            final int position = i;
            model.set(ItemProperties.CLICK_LISTENER,
                    (view) -> { mClickListener.click(position, entry.getIndex()); });
            mModelList.add(new ListItem(NAVIGATION_LIST_ITEM_TYPE_ID, model));
            if (entry.getFavicon() != null) continue;
            final String pageUrl = entry.getUrl();
            if (!requestedUrls.contains(pageUrl)) {
                FaviconHelper.FaviconImageCallback imageCallback =
                        (bitmap, iconUrl) -> onFaviconAvailable(pageUrl, bitmap);
                if (!pageUrl.equals(UrlConstants.HISTORY_URL)) {
                    mFaviconHelper.getLocalFaviconImageForURL(
                            Profile.getLastUsedProfile(), pageUrl, mFaviconSize, imageCallback);
                    requestedUrls.add(pageUrl);
                } else {
                    mModelList.get(i).model.set(ItemProperties.ICON, mHistoryIcon);
                }
            }
        }
    }

    /**
     * Remove the property model.
     */
    void clear() {
        mModelList.clear();
    }

    /**
     * Called when favicon data requested by {@link #initializeFavicons()} is retrieved.
     * @param pageUrl the page for which the favicon was retrieved.
     * @param favicon the favicon data.
     */
    private void onFaviconAvailable(String pageUrl, Bitmap favicon) {
        // This callback can come after the sheet is hidden (which clears modelList).
        // Do nothing if that happens.
        if (mModelList.size() == 0) return;
        for (int i = 0; i < mHistory.getEntryCount(); i++) {
            if (TextUtils.equals(pageUrl, mHistory.getEntryAtIndex(i).getUrl())) {
                Drawable drawable;
                if (favicon == null) {
                    drawable = NewTabPage.isNTPUrl(pageUrl)
                            ? mDefaultIcon
                            : new BitmapDrawable(mIconGenerator.generateIconForUrl(pageUrl));
                } else {
                    drawable = new BitmapDrawable(favicon);
                }
                mModelList.get(i).model.set(ItemProperties.ICON, drawable);
            }
        }
    }

    private String getEntryText(NavigationEntry entry) {
        String entryText = entry.getTitle();
        if (NewTabPage.isNTPUrl(entry.getUrl())) entryText = mNewTabText;
        if (TextUtils.isEmpty(entryText)) entryText = entry.getVirtualUrl();
        if (TextUtils.isEmpty(entryText)) entryText = entry.getUrl();
        return entryText;
    }
}
