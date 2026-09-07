// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.util.ArrayMap;

import org.chromium.base.UserDataHost;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

import java.util.Collection;
import java.util.Map;

/** Registry class to hold multiple TabStateAttributes mapped by Class key. */
@NullMarked
public class TabStateAttributesRegistry extends TabWebContentsUserData {
    // There should be at most 2 instances of TabStateAttributes in the registry.
    private final Map<Class<? extends TabStateAttributes.StoreKey>, TabStateAttributes>
            mAttributesMap = new ArrayMap<>(2);

    public TabStateAttributesRegistry(Tab tab) {
        super(tab);
    }

    /** Returns all {@link TabStateAttributes} instances registered in this registry. */
    public Collection<TabStateAttributes> getAllAttributes() {
        return mAttributesMap.values();
    }

    /**
     * Creates the {@link TabStateAttributes} for the given {@link Tab}.
     *
     * @param tab The Tab reference whose state this is associated with.
     * @param key The key class associated with this attributes instance.
     * @param creationState The creation state of the tab (if it exists).
     */
    public static void createAttributesForTab(
            Tab tab,
            Class<? extends TabStateAttributes.StoreKey> key,
            @Nullable @TabCreationState Integer creationState) {
        UserDataHost host = tab.getUserDataHost();
        TabStateAttributesRegistry registry = host.getUserData(TabStateAttributesRegistry.class);
        if (registry == null) {
            registry =
                    host.setUserData(
                            TabStateAttributesRegistry.class, new TabStateAttributesRegistry(tab));
        }
        registry.put(key, new TabStateAttributes(tab, creationState));
    }

    /** Returns {@link TabStateAttributes} for a {@link Tab} associated with the key class. */
    public static @Nullable TabStateAttributes getAttributesFor(
            @Nullable Tab tab, Class<? extends TabStateAttributes.StoreKey> key) {
        if (tab == null || tab.isDestroyed()) return null;
        UserDataHost host = tab.getUserDataHost();
        TabStateAttributesRegistry registry = host.getUserData(TabStateAttributesRegistry.class);
        if (registry == null) return null;
        return registry.get(key);
    }

    public static void clearForTesting(Tab tab) {
        tab.getUserDataHost().removeUserData(TabStateAttributesRegistry.class);
    }

    @Override
    public void initWebContents(WebContents webContents) {
        for (TabStateAttributes attr : mAttributesMap.values()) {
            attr.beginTracking(webContents);
        }
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {
        for (TabStateAttributes attr : mAttributesMap.values()) {
            attr.cleanupWebContents(webContents);
        }
    }

    @Override
    protected void destroyInternal() {
        for (TabStateAttributes attr : mAttributesMap.values()) {
            attr.destroy();
        }
        mAttributesMap.clear();
    }

    private void put(
            Class<? extends TabStateAttributes.StoreKey> key, TabStateAttributes attributes) {
        mAttributesMap.put(key, attributes);
        WebContents wc = getWebContents();
        if (wc != null) {
            attributes.beginTracking(wc);
        }
    }

    private @Nullable TabStateAttributes get(Class<? extends TabStateAttributes.StoreKey> key) {
        return mAttributesMap.get(key);
    }
}
