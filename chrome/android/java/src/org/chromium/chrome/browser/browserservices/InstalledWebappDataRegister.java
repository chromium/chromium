// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Records whether Chrome has data relevant to an installed webapp (TWA or WebAPK).
 *
 * <p>Lifecycle: Most of the data used by this class modifies the underlying {@link
 * SharedPreferences} (which are global and preserved across Chrome restarts).
 */
@NullMarked
public class InstalledWebappDataRegister {

    /** The shared preferences file name. If you modify this you'll have to migrate old data. */
    private static final String PREFS_FILE = "trusted_web_activity_client_apps";

    /**
     * The key to the set of UIDs stored as strings in shared preferences. If you modify this you'll
     * have to migrate old data.
     */
    private static final String UIDS_KEY = "trusted_web_activity_uids";

    /** The key to the set of package names stored as strings in shared preferences. */
    private static final String PACKAGES_KEY = "trusted_web_activity_packages";

    /* Preferences unique to this class. */
    private static @Nullable SharedPreferences sPreferences;

    private InstalledWebappDataRegister() {}

    private static SharedPreferences getPreferences() {
        // Not threadsafe, but doesn't matter as this is idempotent.
        if (sPreferences == null) {
            sPreferences =
                    ContextUtils.getApplicationContext()
                            .getSharedPreferences(PREFS_FILE, Context.MODE_PRIVATE);
        }
        return sPreferences;
    }

    public static void setPreferencesForTesting(@Nullable SharedPreferences sharedPreferences) {
        sPreferences = sharedPreferences;
    }

    // Trigger a Preferences read in a background thread to try to load the Preferences file
    // before we need it.
    public static void prefetchPreferences() {
        PostTask.postTask(
                TaskTraits.BEST_EFFORT,
                () -> {
                    migrateOldDataIfNeeded();
                    getPackages();
                });
    }

    /* package */ static Set<String> getPackages() {
        return new HashSet<>(getPreferences().getStringSet(PACKAGES_KEY, Collections.emptySet()));
    }

    private static void setPackages(Set<String> packages) {
        getPreferences().edit().putStringSet(PACKAGES_KEY, packages).apply();
    }

    public static void migrateOldDataIfNeeded() {
        SharedPreferences prefs = getPreferences();
        if (!prefs.contains(UIDS_KEY)) return;

        Set<String> uids = getUids();
        Set<String> packages = getPackages();

        SharedPreferences.Editor editor = prefs.edit();

        for (String uidStr : uids) {
            int uid;
            try {
                uid = Integer.parseInt(uidStr);
            } catch (NumberFormatException e) {
                editor.remove(uidStr + ".packageName");
                editor.remove(uidStr + ".appName");
                editor.remove(uidStr + ".domain");
                editor.remove(uidStr + ".origin");
                continue;
            }
            String packageName = prefs.getString(createPackageNameKey(uid), null);
            String appName = prefs.getString(createAppNameKey(uid), null);
            Set<String> domains = prefs.getStringSet(createDomainKey(uid), Collections.emptySet());
            Set<String> origins = prefs.getStringSet(createOriginKey(uid), Collections.emptySet());

            if (packageName != null) {
                packages.add(packageName);
                editor.putString(createAppNameKey(packageName), appName);

                Set<String> newDomains =
                        new HashSet<>(
                                prefs.getStringSet(
                                        createDomainKey(packageName), Collections.emptySet()));
                newDomains.addAll(domains);
                editor.putStringSet(createDomainKey(packageName), newDomains);

                Set<String> newOrigins =
                        new HashSet<>(
                                prefs.getStringSet(
                                        createOriginKey(packageName), Collections.emptySet()));
                newOrigins.addAll(origins);
                editor.putStringSet(createOriginKey(packageName), newOrigins);
            }

            // Remove old data
            editor.remove(createPackageNameKey(uid));
            editor.remove(createAppNameKey(uid));
            editor.remove(createDomainKey(uid));
            editor.remove(createOriginKey(uid));
        }

        editor.putStringSet(PACKAGES_KEY, packages);
        editor.remove(UIDS_KEY);
        editor.apply();
    }

    /**
     * Saves to Preferences that the app has the application name |appName| and when it is removed
     * or cleared, we should consider doing the same with Chrome data relevant to |origin|. |domain|
     * is stored as well in order to not have to derive it from origin while handling uninstallation
     * or data clear, since that would require loading native libraries.
     */
    public static void registerPackageForOrigin(
            String appName, String packageName, @Nullable String domain, Origin origin) {
        // Store the Package Name in the new Preferences.
        Set<String> packages = getPackages();
        packages.add(packageName);
        setPackages(packages);

        SharedPreferences.Editor editor = getPreferences().edit();

        // Write to new keys
        editor.putString(createAppNameKey(packageName), appName);
        writeToSet(editor, createDomainKey(packageName), domain);
        writeToSet(editor, createOriginKey(packageName), origin.toString());

        editor.apply();
    }

    private static void writeToSet(
            SharedPreferences.Editor editor, String key, @Nullable String newElement) {
        Set<String> set = new HashSet<>(getPreferences().getStringSet(key, Collections.emptySet()));
        set.add(newElement);
        editor.putStringSet(key, set);
    }

    /* package */ static Set<String> getUids() {
        // We try to ensure that this is loaded on a background thread before it is needed (see
        // constructor), but if the load hasn't completed, disable StrictMode so we don't crash.
        return new HashSet<>(getPreferences().getStringSet(UIDS_KEY, Collections.emptySet()));
    }

    public static void removePackage(String packageName) {
        Set<String> packages = getPackages();
        packages.remove(packageName);
        setPackages(packages);

        SharedPreferences.Editor editor = getPreferences().edit();
        editor.remove(createAppNameKey(packageName));
        editor.remove(createDomainKey(packageName));
        editor.remove(createOriginKey(packageName));
        editor.apply();
    }

    /* package */ static boolean chromeHoldsDataForPackage(String packageName) {
        return getPackages().contains(packageName);
    }

    /* package */ static @Nullable String getAppNameForRegisteredPackage(String packageName) {
        return getPreferences().getString(createAppNameKey(packageName), null);
    }

    /* package */ static Set<String> getDomainsForRegisteredPackage(String packageName) {
        return getPreferences().getStringSet(createDomainKey(packageName), Collections.emptySet());
    }

    /* package */ static Set<String> getOriginsForRegisteredPackage(String packageName) {
        return getPreferences().getStringSet(createOriginKey(packageName), Collections.emptySet());
    }

    // Methods below create the Preferences keys to access the data associated with given app.
    // If you modify any of them you'll have to migrate old data.

    private static String createAppNameKey(String packageName) {
        return packageName + ".appName";
    }

    private static String createDomainKey(String packageName) {
        return packageName + ".domain";
    }

    private static String createOriginKey(String packageName) {
        return packageName + ".origin";
    }

    private static String createAppNameKey(int uid) {
        return uid + ".appName";
    }

    private static String createPackageNameKey(int uid) {
        return uid + ".packageName";
    }

    private static String createDomainKey(int uid) {
        return uid + ".domain";
    }

    private static String createOriginKey(int uid) {
        return uid + ".origin";
    }
}
