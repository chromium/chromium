// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.SharedPreferences;

import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.net.GURLUtils;

import java.util.HashSet;
import java.util.Set;

/**
 * This class is used to manage permissions for the WebView's Geolocation JavaScript API.
 *
 * Callbacks are posted on the UI thread.
 */
public final class AwGeolocationPermissions {

    private static final String PREF_PREFIX =
            "AwGeolocationPermissions%";
    private final SharedPreferences mSharedPreferences;

    /** See {@link android.webkit.GeolocationPermissions}. */
    public interface Callback {
        /* See {@link android.webkit.GeolocationPermissions}. */
        public void invoke(String origin, boolean allow, boolean retain);
    }

    public AwGeolocationPermissions(SharedPreferences sharedPreferences) {
        mSharedPreferences = sharedPreferences;
    }

    /**
     * Set one origin to be allowed.
     */
    public void allow(String origin) {
        String key = getOriginKey(origin);
        if (key != null) {
            mSharedPreferences.edit().putBoolean(key, true).apply();
        }
    }

    /**
     * Set one origin to be denied.
     */
    public void deny(String origin) {
        String key = getOriginKey(origin);
        if (key != null) {
            mSharedPreferences.edit().putBoolean(key, false).apply();
        }
    }

    /**
     * Clear the stored permission for a particular origin.
     */
    public void clear(String origin) {
        String key = getOriginKey(origin);
        if (key != null) {
            mSharedPreferences.edit().remove(key).apply();
        }
    }

    /**
     * Clear stored permissions for all origins.
     */
    public void clearAll() {
        SharedPreferences.Editor editor = null;
        for (String name : mSharedPreferences.getAll().keySet()) {
            if (name.startsWith(PREF_PREFIX)) {
                if (editor == null) {
                    editor = mSharedPreferences.edit();
                }
                editor.remove(name);
            }
        }
        if (editor != null) {
            editor.apply();
        }
    }

    /**
     * Synchronous method to get if an origin is set to be allowed.
     */
    public boolean isOriginAllowed(String origin) {
        return mSharedPreferences.getBoolean(getOriginKey(origin), false);
    }

    /**
     * Returns true if the origin is either set to allowed or denied.
     */
    public boolean hasOrigin(String origin) {
        return mSharedPreferences.contains(getOriginKey(origin));
    }

    /**
     * Asynchronous method to get if an origin set to be allowed.
     */
    public void getAllowed(String origin, final org.chromium.base.Callback<Boolean> callback) {
        final boolean finalAllowed = isOriginAllowed(origin);
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> callback.onResult(finalAllowed));
    }

    /**
     * Async method to get the domains currently allowed or denied.
     */
    public void getOrigins(final org.chromium.base.Callback<Set<String>> callback) {
        final Set<String> origins = new HashSet<String>();
        for (String name : mSharedPreferences.getAll().keySet()) {
            if (name.startsWith(PREF_PREFIX)) {
                origins.add(name.substring(PREF_PREFIX.length()));
            }
        }
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> callback.onResult(origins));
    }

    /**
     * Get the domain of an URL using the GURL library.
     */
    private String getOriginKey(String url) {
        String origin = GURLUtils.getOrigin(url);
        if (origin.isEmpty()) {
            return null;
        }

        return PREF_PREFIX + origin;
    }

    /* package */
    static void migrateGeolocationPreferences(
            SharedPreferences oldPrefs, SharedPreferences newPrefs) {
        SharedPreferences.Editor oldPrefsEditor = oldPrefs.edit();

        SharedPreferences.Editor newPrefsEditor = newPrefs.edit();

        for (String name : oldPrefs.getAll().keySet()) {
            if (name.startsWith(AwGeolocationPermissions.PREF_PREFIX)) {
                newPrefsEditor.putBoolean(name, oldPrefs.getBoolean(name, false)).apply();
                oldPrefsEditor.remove(name).apply();
            }
        }
    }
}
