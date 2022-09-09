// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.Nullable;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;

import java.util.HashMap;
import java.util.Map;

/**
 * Per-tab storage that holds a map of attributes. Allows other classes to access
 * the attribute without having it directly hang on to a tab.
 */
public class TabAttributes implements UserData {
    private static final Class<TabAttributes> USER_DATA_KEY = TabAttributes.class;

    private final Map<String, Object> mAttributes = new HashMap<>();

    // Null object used to differentiate the uninitialized attributes from those explicitly
    // set to |null|.
    private static final Object NULL_VALUE = new Object();

    public static TabAttributes from(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        TabAttributes attrs = host.getUserData(USER_DATA_KEY);
        return attrs != null ? attrs : host.setUserData(USER_DATA_KEY, new TabAttributes());
    }

    private TabAttributes() {}

    /**
     * Gets the attribute of the Tab.
     * @param key Name of the attribute.
     */
    @Nullable
    @SuppressWarnings("unchecked")
    public <T> T get(@TabAttributeKeys String key) {
        Object value = mAttributes.get(key);
        return value != NULL_VALUE ? (T) value : null;
    }

    /**
     * Gets the attribute of the Tab.
     * @param key Name of the attribute.
     * @param defaultValue Default attribute to return if the attribute has not been set.
     *     Note that the attribute that has been set to {@code null} is also regarded
     *     as <b>set</b>, therefore returns {@code null} not the default value.
     */
    @Nullable
    public <T> T get(@TabAttributeKeys String key, T defaultValue) {
        return mAttributes.containsKey(key) ? get(key) : defaultValue;
    }

    /**
     * Sets the attribute of the Tab.
     * <p>
     * Note that {@code value} can be {@code null}, which will make the attribute have
     * an explicit {@code null}. {@link getValue(String, T)} will start returning
     * {@code null} rather than the passed default value.
     * @param key Name of the attribute.
     * @param value Attribute to set.
     */
    public <T> void set(@TabAttributeKeys String key, @Nullable T value) {
        mAttributes.put(key, value == null ? NULL_VALUE : value);
    }

    /**
     * Clears the attribute of the Tab. {@link get()} returns {@code null} after the attribute
     * is cleared.
     * <p>
     * @param key Name of the attribute.
     */
    public void clear(@TabAttributeKeys String key) {
        mAttributes.remove(key);
    }
}
