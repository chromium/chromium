// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.pseudotab;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.base.LifetimeAssert;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;

/**
 * Cache for attributes of {@link PseudoTab} to be available before native is ready.
 */
public class TabAttributeCache {
    private static final String PREFERENCES_NAME = "tab_attribute_cache";
    private static SharedPreferences sPref;
    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    private static SharedPreferences getSharedPreferences() {
        if (sPref == null) {
            sPref = ContextUtils.getApplicationContext().getSharedPreferences(
                    PREFERENCES_NAME, Context.MODE_PRIVATE);
        }
        return sPref;
    }

    /**
     * Create a TabAttributeCache instance to observe tab attribute changes.
     *
     * Note that querying tab attributes doesn't rely on having an instance.
     * @param tabModelSelector The {@link TabModelSelector} to observe.
     */
    public TabAttributeCache(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(mTabModelSelector) {
            @Override
            public void onUrlUpdated(Tab tab) {
                if (tab.isIncognito()) return;
                String url = tab.getUrl();
                cacheUrl(tab.getId(), url);
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                if (tab.isIncognito()) return;
                String title = tab.getTitle();
                cacheTitle(tab.getId(), title);
            }

            @Override
            public void onRootIdChanged(Tab tab, int newRootId) {
                if (tab.isIncognito()) return;
                assert newRootId == tab.getRootId();
                cacheRootId(tab.getId(), newRootId);
            }
        };

        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void tabClosureCommitted(Tab tab) {
                int id = tab.getId();
                getSharedPreferences()
                        .edit()
                        .remove(getUrlKey(id))
                        .remove(getTitleKey(id))
                        .remove(getRootIdKey(id))
                        .apply();
            }
        };

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabStateInitialized() {
                // TODO(wychen): after this cache is enabled by default, we only need to populate it
                // once.
                TabModelFilter filter =
                        mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(false);
                for (int i = 0; i < filter.getCount(); i++) {
                    Tab tab = filter.getTabAt(i);
                    cacheUrl(tab.getId(), tab.getUrl());
                    cacheTitle(tab.getId(), tab.getTitle());
                    cacheRootId(tab.getId(), tab.getRootId());
                }
                filter.addObserver(mTabModelObserver);
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    private static String getTitleKey(int id) {
        return id + "_title";
    }

    /**
     * Get the title of a {@link PseudoTab}.
     * @param id The ID of the {@link PseudoTab}.
     * @return The title
     */
    public static String getTitle(int id) {
        return getSharedPreferences().getString(getTitleKey(id), "");
    }

    private static void cacheTitle(int id, String title) {
        getSharedPreferences().edit().putString(getTitleKey(id), title).apply();
    }

    /**
     * Set the title of a {@link PseudoTab}. Only for testing.
     * @param id The ID of the {@link PseudoTab}.
     * @param title The title
     */
    static void setTitleForTesting(int id, String title) {
        cacheTitle(id, title);
    }

    private static String getUrlKey(int id) {
        return id + "_url";
    }

    /**
     * Get the URL of a {@link PseudoTab}.
     * @param id The ID of the {@link PseudoTab}.
     * @return The URL
     */
    public static String getUrl(int id) {
        return getSharedPreferences().getString(getUrlKey(id), "");
    }

    private static void cacheUrl(int id, String url) {
        getSharedPreferences().edit().putString(getUrlKey(id), url).apply();
    }

    /**
     * Set the URL of a {@link PseudoTab}.
     * @param id The ID of the {@link PseudoTab}.
     * @param url The URL
     */
    static void setUrlForTesting(int id, String url) {
        cacheUrl(id, url);
    }

    private static String getRootIdKey(int id) {
        return id + "_rootID";
    }

    /**
     * Get the root ID of a {@link PseudoTab}.
     * @param id The ID of the {@link PseudoTab}.
     * @return The root ID
     */
    public static int getRootId(int id) {
        return getSharedPreferences().getInt(getRootIdKey(id), Tab.INVALID_TAB_ID);
    }

    private static void cacheRootId(int id, int rootId) {
        getSharedPreferences().edit().putInt(getRootIdKey(id), rootId).apply();
    }

    /**
     * Set the root ID for a {@link PseudoTab}.
     * @param id The ID of the {@link PseudoTab}.
     * @param rootId The root ID
     */
    static void setRootIdForTesting(int id, int rootId) {
        cacheRootId(id, rootId);
    }

    /**
     * Clear everything in the storage.
     */
    static void clearAllForTesting() {
        getSharedPreferences().edit().clear().apply();
    }

    /**
     * Remove all the observers.
     */
    public void destroy() {
        mTabModelSelectorTabObserver.destroy();
        mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(false).removeObserver(
                mTabModelObserver);
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }
}
