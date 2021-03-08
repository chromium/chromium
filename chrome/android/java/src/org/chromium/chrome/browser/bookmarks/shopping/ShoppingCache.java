// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.shopping;

import android.content.Context;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.UserData;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.javascript.WebContextFetcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.content_public.browser.WebContents;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

import androidx.annotation.Nullable;

public class ShoppingCache {
    private static final String LAST_UPDATED_TIME_STAMP_NAME = "lastUpdatedTime";
    private static final long UPDATE_NEEDED_TIME = 1000 * 60 * 60; // One hour

    public static class LocalCacheHelper implements UserData {
        public static final Class<LocalCacheHelper> USER_DATA_KEY = LocalCacheHelper.class;

        private static BookmarkModel sBookmarkModel;
        private static BookmarkId sShoppingFolder;
        private BookmarkBridge.BookmarkModelObserver mBookmarkModelObserver;

        private Tab mTab;

        private LocalCacheHelper(Tab tab) {
            mTab = tab;

            mBookmarkModelObserver = new BookmarkBridge.BookmarkModelObserver() {
                @Override
                public void bookmarkModelChanged() {

                }

                @Override
                public void bookmarkNodeAdded(BookmarkBridge.BookmarkItem parent, int index) {
                    // Might still not have found the shopping folder...
                    if ((sShoppingFolder == null || sShoppingFolder.getId() < 0) &&
                            sBookmarkModel.isBookmarkModelLoaded()) {
                        sShoppingFolder = BookmarkUtils.findShoppingFolder(sBookmarkModel);
                    }

                    if (parent.getId().getId() != sShoppingFolder.getId()) return;
                    BookmarkId addedId = sBookmarkModel.getChildAt(parent.getId(), index);
                    onShoppingBookmarkAdded(addedId);
                }
            };

            if (sBookmarkModel == null) {
                sBookmarkModel = new BookmarkModel();
                sBookmarkModel.finishLoadingBookmarkModel(() -> {
                    sShoppingFolder = BookmarkUtils.findShoppingFolder(sBookmarkModel);
                });
            }

            if (sShoppingFolder == null && sBookmarkModel.isBookmarkModelLoaded()) {
                sShoppingFolder = BookmarkUtils.findShoppingFolder(sBookmarkModel);
            }
            sBookmarkModel.addObserver(mBookmarkModelObserver);
        }

        private void onShoppingBookmarkAdded(BookmarkId id) {
            BookmarkBridge.BookmarkItem item = sBookmarkModel.getBookmarkById(id);
            if (!mTab.getUrl().equals(item.getUrl())) return;
            getShoppingInfoFromPage(mTab.getContext(), mTab.getWebContents(), id);
        }

        public static void createForTab(Tab tab) {
            tab.getUserDataHost().setUserData(USER_DATA_KEY, new LocalCacheHelper(tab));
        }
    }

    /**
     * Read the cache and optionally update based on a new response and a list of the current
     * shopping bookmark ids.
     * @param response The new response from the server.
     * @param currentBookmarkIds The list of current bookmark ids in the shopping folder.
     * @return A map of bookmark id to meta.
     */
    public static Map<Long, JSONObject> updateAndGetCache(
            @Nullable String response, @Nullable List<BookmarkId> currentBookmarkIds) {
        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();

        JSONArray cachedItems = null;
        if (prefs.contains(ChromePreferenceKeys.SHOPPING_CACHE)) {
            try {
                cachedItems =
                        new JSONArray(prefs.readString(ChromePreferenceKeys.SHOPPING_CACHE, ""));
            } catch (JSONException ex) {
                // Failed to read cache, clear it.
                prefs.removeKeySync(ChromePreferenceKeys.SHOPPING_CACHE);
            }
        }

        // Pull the previously stored values into a map to update from the response.
        Map<Long, JSONObject> cachedMap = new HashMap<>();
        if (cachedItems != null) {
            for (int i = 0; i < cachedItems.length(); i++) {
                try {
                    JSONObject curItem = cachedItems.getJSONObject(i);
                    cachedMap.put(curItem.getLong("id"), curItem);
                } catch (JSONException ex) {
                    // Ignore the item that failed to parse, it won't be rewritten to the cache.
                }
            }
        }

        // Remove items from the cache that the user has removed.
        if (currentBookmarkIds != null) {
            Set<Long> curIds = new HashSet<>();
            List<Long> itemsToRemove = new ArrayList<>();
            for (BookmarkId id : currentBookmarkIds) curIds.add(id.getId());
            for (Map.Entry<Long, JSONObject> item : cachedMap.entrySet()) {
                if (!curIds.contains(item.getKey())) itemsToRemove.add(item.getKey());
            }
            for (Long id : itemsToRemove) cachedMap.remove(id);
        }

        if (response != null) {
            try {
                JSONObject object = new JSONObject(response);

                JSONArray items = object.getJSONArray("items");
                for (int i = 0; i < items.length(); i++) {

                    JSONObject curItem = items.getJSONObject(i);
                    long bookmarkId = curItem.has("id") ? curItem.getLong("id") : -1;

                    // Check if there was an existing item and prevent nulling out of useful
                    // information like image, price, and title.
                    JSONObject existingItem = cachedMap.get(bookmarkId);
                    if (existingItem != null) {
                        JSONObject newCardData = safeGetJSONObject(curItem, "card");
                        JSONObject existingCardData = safeGetJSONObject(existingItem, "card");

                        String imageUrl = safeGetJSONString(newCardData, "imageUrl");
                        String existingImageUrl = safeGetJSONString(existingCardData, "imageUrl");
                        if (imageUrl == null && existingCardData != null) {
                            newCardData.put("imageUrl", existingImageUrl);
                        }

                        String title = safeGetJSONString(newCardData, "title");
                        String existingTitle = safeGetJSONString(existingCardData, "title");
                        if (title == null && existingCardData != null) {
                            newCardData.put("title", existingTitle);
                        }
                    }
                    cachedMap.put(bookmarkId, curItem);
                    curItem.put(LAST_UPDATED_TIME_STAMP_NAME, System.currentTimeMillis());

                }

            } catch (JSONException e) {
                // Couldn't parse the response.
                android.util.Log.w("mdjones", response);
            }
        }

        // If either the response or curent bookmarks weren't null, rewrite the cache.
        if (response != null || currentBookmarkIds != null) {
            JSONArray newCache = new JSONArray();
            for (Map.Entry<Long, JSONObject> entry : cachedMap.entrySet()) {
                newCache.put(entry.getValue());
            }
            prefs.writeString(ChromePreferenceKeys.SHOPPING_CACHE, newCache.toString());
        }

        return cachedMap;
    }

    public static boolean itemNeedsUpdate(JSONObject item) {
        // First check if the data is stale.
        try {
            long time = item.getLong(LAST_UPDATED_TIME_STAMP_NAME);
            if (System.currentTimeMillis() - time > UPDATE_NEEDED_TIME) {
                return true;
            }

        } catch (JSONException ex) {
            return true;
        }

        // If the product was missing important information, require an update.
        JSONObject cardData = safeGetJSONObject(item, "card");
        String imageUrl = ShoppingCache.safeGetJSONString(cardData, "imageUrl");


        if (imageUrl == null) {
            return true;
        }

        JSONObject dataObject = safeGetJSONObject(cardData, "data");
        JSONObject priceDataObject = safeGetJSONObject(dataObject, "priceData");
        JSONObject curPriceObject = safeGetJSONObject(priceDataObject, "currentPrice");
        if (curPriceObject == null || !curPriceObject.has("amountInMicros")) {
            return true;
        }

        return false;
    }

    public static void getShoppingInfoFromPage(
            Context context, WebContents webContents, BookmarkId bookmarkedId) {
        String script;
        InputStream in = context.getResources().openRawResource(R.raw.query_meta_tags);
        int size = 0;
        byte[] buffer = new byte[512];
        try {
            script = "";
            while ((size = in.read(buffer, 0, 512)) > 0) script += new String(buffer, 0, size);
        } catch (IOException e) {
            script = null;
        }

        WebContextFetcher.fetchContextWithJavascriptUpdated(script, (fetcherResponse) -> {
            try {
                Map<String, String> map = fetcherResponse.context;

                // Build an object that looks the same as a response from the api so we can use the same
                // logic everywhere.
                JSONObject root = new JSONObject();
                JSONArray items = new JSONArray();
                JSONObject itemObject = new JSONObject();
                JSONObject cardObject = new JSONObject();
                root.put("items", items);
                items.put(itemObject);
                itemObject.put("card", cardObject);
                itemObject.put("id", bookmarkedId.getId());

                for (Map.Entry<String, String> entry : map.entrySet()) {
                    switch (entry.getKey().toLowerCase(Locale.getDefault())) {
                        case "title":
                            cardObject.put("title", entry.getValue());
                            break;
                        case "image":
                            cardObject.put("imageUrl", entry.getValue());
                            break;
                        case "price:currency":

                            // if price is not also available, break.
                            if (map.get("price:amount") == null) break;

                            // Don't attach this to the root until we know we have valid data.
                            JSONObject dataObj = new JSONObject();

                            JSONObject priceData = new JSONObject();
                            dataObj.put("priceData", priceData);

                            JSONObject curPrice = new JSONObject();
                            priceData.put("currentPrice", curPrice);

                            long micros = 0;
                            try {
                                micros = (long) (Float.parseFloat(map.get("price:amount")) * 1e6);
                            } catch (NumberFormatException e) {
                                break;
                            }
                            curPrice.put("amountInMicros", micros + "");
                            curPrice.put("currencyCode", map.get("price:currency"));

                            cardObject.put("data", dataObj);

                            break;
                        default:
                            break;
                    }
                }

                // Dump this item into the cache.
                updateAndGetCache(root.toString(), null);

            } catch (JSONException e) {
                // noop
            }
        }, webContents.getMainFrame());
    }

    public static JSONObject safeGetJSONObject(JSONObject obj, String name) {
        if (obj == null) return null;
        try {
            return obj.has(name) ? obj.getJSONObject(name) : null;
        } catch (JSONException ex) {
            // noop
        }
        return null;
    }

    public static String safeGetJSONString(JSONObject obj, String name) {
        if (obj == null) return null;
        try {
            return obj.has(name) ? obj.getString(name) : null;
        } catch (JSONException ex) {
            // noop
        }
        return null;
    }
}
