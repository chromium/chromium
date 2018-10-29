// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Records whether Chrome has data relevant to a Trusted Web Activity Client.
 *
 * Lifecycle: Most of the data used by this class modifies the underlying {@link SharedPreferences}
 * (which are global and preserved across Chrome restarts).
 * Thread safety: This object should only be accessed on a single thread at any time.
 */
public class ClientAppDataRegister {
    private static final String PREFS_FILE = "trusted_web_activity_client_apps";
    private static final String UIDS_KEY = "trusted_web_activity_uids";

    /* Preferences unique to this class. */
    private final SharedPreferences mPreferences;

    /** Creates a ClientAppDataRegister. */
    public ClientAppDataRegister() {
        try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
            mPreferences = ContextUtils.getApplicationContext()
                    .getSharedPreferences(PREFS_FILE, Context.MODE_PRIVATE);
        }
    }

    /**
     * Saves to Preferences that the app with |uid| has the application name |appName| and when it
     * is removed or cleared, we should consider doing the same with Chrome data relevant to
     * |domain|.
     */
    /* package */ void registerPackageForDomain(int uid, String appName, String domain) {
        // Store the UID in the main Chrome Preferences.
        Set<String> uids = getUids();
        uids.add(String.valueOf(uid));
        setUids(uids);

        // Store the package name for the UID.
        mPreferences.edit().putString(createAppNameKey(uid), appName).apply();

        // Store the domain for the UID.
        String key = createDomainKey(uid);
        Set<String> domains = new HashSet<>(mPreferences.getStringSet(key, Collections.emptySet()));
        domains.add(domain);
        mPreferences.edit().putStringSet(key, domains).apply();
    }

    private void setUids(Set<String> uids) {
        mPreferences.edit().putStringSet(UIDS_KEY, uids).apply();
    }

    private Set<String> getUids() {
        return new HashSet<>(mPreferences.getStringSet(UIDS_KEY, Collections.emptySet()));
    }

    /* package */ void removePackage(int uid) {
        Set<String> uids = getUids();
        uids.remove(String.valueOf(uid));
        setUids(uids);

        mPreferences.edit().putString(createAppNameKey(uid), null).apply();
        mPreferences.edit().putStringSet(createDomainKey(uid), null).apply();
    }

    /* package */ boolean chromeHoldsDataForPackage(int uid) {
        return getUids().contains(String.valueOf(uid));
    }

    /**
     * Gets the package name that was previously registered for the uid.
     */
    /* package */ @Nullable String getAppNameForRegisteredUid(int uid) {
        return mPreferences.getString(createAppNameKey(uid), null);
    }

    /**
     * Gets all the domains that have been registered for the uid.
     * Do not modify the set returned by this method.
     */
    /* package */ Set<String> getDomainsForRegisteredUid(int uid) {
        return mPreferences.getStringSet(createDomainKey(uid), Collections.emptySet());
    }

    /**
     * Creates the Preferences key to access the app name.
     * If you modify this you'll have to migrate old data.
     */
    private static String createAppNameKey(int uid) {
        return uid + ".appName";
    }

    /**
     * Creates the Preferences key to access the set of domains for an app.
     * If you modify this you'll have to migrate old data.
     */
    private static String createDomainKey(int uid) {
        return uid + ".domain";
    }
}
