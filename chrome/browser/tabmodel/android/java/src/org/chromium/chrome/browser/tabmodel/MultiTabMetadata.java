// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Bundle;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;

import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

/** This class is a container for the metadata of multiple tabs that are being reparented. */
@NullMarked
public class MultiTabMetadata {
    private static final String KEY_TAB_IDS = "MultiTabReparentingIdsKey";
    private static final String KEY_TAB_URLS = "MultiTabReparentingUrlsKey";
    private static final String KEY_IS_INCOGNITO = "MultiTabReparentingIsIncognitoKey";
    private static final String KEY_IS_PINNED = "MultiTabReparentingIsPinnedKey";

    public final ArrayList<Integer> tabIds = new ArrayList<>();
    public final ArrayList<String> urls = new ArrayList<>();
    public final boolean[] isPinned;
    public final boolean isIncognito;

    private MultiTabMetadata(@Nullable List<Tab> tabs) {
        if (tabs == null || tabs.isEmpty()) {
            isIncognito = false;
            isPinned = new boolean[0];
            return;
        }
        isPinned = new boolean[tabs.size()];
        for (int i = 0; i < tabs.size(); i++) {
            Tab tab = tabs.get(i);
            tabIds.add(tab.getId());
            urls.add(tab.getUrl().getSpec());
            isPinned[i] = tab.getIsPinned();
        }
        isIncognito = tabs.get(0).isIncognito();
    }

    private MultiTabMetadata(
            ArrayList<Integer> tabIds,
            ArrayList<String> urls,
            boolean[] isPinned,
            boolean isIncognito) {
        this.tabIds.addAll(tabIds);
        this.urls.addAll(urls);
        this.isPinned = isPinned;
        this.isIncognito = isIncognito;
    }

    /**
     * Creates a new {@link MultiTabMetadata} instance.
     *
     * @param tabs The list of {@link Tab} objects.
     * @return A new {@link MultiTabMetadata} object.
     */
    public static MultiTabMetadata create(@Nullable List<Tab> tabs) {
        return new MultiTabMetadata(tabs);
    }

    /**
     * Creates a new {@link MultiTabMetadata} instance for testing.
     *
     * @param tabIds The list of tab IDs.
     * @param urls The list of URLs.
     * @param isIncognito Whether the tabs are in incognito mode.
     * @return A new {@link MultiTabMetadata} object.
     */
    public static MultiTabMetadata createForTesting(
            ArrayList<Integer> tabIds,
            ArrayList<String> urls,
            boolean[] isPinned,
            boolean isIncognito) {
        return new MultiTabMetadata(tabIds, urls, isPinned, isIncognito);
    }

    /** Returns A {@link Bundle} containing the metadata. */
    public Bundle toBundle() {
        Bundle bundle = new Bundle();
        bundle.putIntegerArrayList(KEY_TAB_IDS, tabIds);
        bundle.putStringArrayList(KEY_TAB_URLS, urls);
        bundle.putBooleanArray(KEY_IS_PINNED, isPinned);
        bundle.putBoolean(KEY_IS_INCOGNITO, isIncognito);
        return bundle;
    }

    public static @Nullable MultiTabMetadata maybeCreateFromBundle(@Nullable Bundle bundle) {
        if (bundle == null) return null;
        ArrayList<Integer> tabIds = bundle.getIntegerArrayList(KEY_TAB_IDS);
        ArrayList<String> urls = bundle.getStringArrayList(KEY_TAB_URLS);
        boolean[] isPinned = bundle.getBooleanArray(KEY_IS_PINNED);
        if (!isBundleValid(tabIds, urls, assumeNonNull(isPinned), bundle)) return null;
        assert isPinned != null;
        return new MultiTabMetadata(tabIds, urls, isPinned, bundle.getBoolean(KEY_IS_INCOGNITO));
    }

    @Override
    public boolean equals(Object other) {
        if (this == other) return true;
        if (other instanceof MultiTabMetadata that) {
            return Objects.equals(tabIds, that.tabIds)
                    && Objects.equals(urls, that.urls)
                    && Arrays.equals(isPinned, that.isPinned)
                    && isIncognito == that.isIncognito;
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(this.tabIds, this.urls, Arrays.hashCode(this.isPinned), this.isIncognito);
    }

    @Override
    public String toString() {
        return "MultiTabMetadata{tabIds="
                + tabIds
                + ", urls="
                + urls
                + ", isPinned="
                + Arrays.toString(isPinned)
                + ", isIncognito="
                + isIncognito
                + '}';
    }

    public static String getTabIdsKeyForTesting() {
        return KEY_TAB_IDS;
    }

    public static String getTabUrlsKeyForTesting() {
        return KEY_TAB_URLS;
    }

    public static String getIsIncognitoKeyForTesting() {
        return KEY_IS_INCOGNITO;
    }

    public static String getIsPinnedKeyForTesting() {
        return KEY_IS_PINNED;
    }

    @Contract("null, _, _, _ -> false; _, null, _, _ -> false; _, _, null, _ -> false")
    private static boolean isBundleValid(
            @Nullable ArrayList<Integer> tabIds,
            @Nullable ArrayList<String> urls,
            boolean[] isPinned,
            Bundle bundle) {
        return areLengthsValid(tabIds, urls, isPinned) && bundle.containsKey(KEY_IS_INCOGNITO);
    }

    private static boolean areLengthsValid(
            @Nullable ArrayList<Integer> tabIds,
            @Nullable ArrayList<String> urls,
            boolean[] isPinned) {
        if (tabIds == null || urls == null || isPinned == null) return false;
        return tabIds.size() == urls.size() && tabIds.size() == isPinned.length;
    }
}
