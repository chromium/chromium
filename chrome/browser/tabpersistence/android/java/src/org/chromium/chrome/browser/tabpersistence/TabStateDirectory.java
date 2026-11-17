// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabpersistence;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Locale;

/**
 * Manages the directory where tab state is saved.
 *
 * <p>TODO(crbug.com/40136597): Deduplicate code between tabbed mode and custom tabs.
 */
@NullMarked
public class TabStateDirectory {
    private static final String TAG = "tabpersistence";

    /** The name of the base directory where the state is saved. */
    private static final String BASE_STATE_FOLDER = "tabs";

    /** The name of the directory where the state for tabbed mode is saved. */
    @VisibleForTesting public static final String TABBED_MODE_DIRECTORY = "0";

    /** The name of the directory where the state for custom tabs is saved. */
    public static final String CUSTOM_TABS_DIRECTORY = "custom_tabs";

    /** The name of the directory where the state for per-app custom tabs is saved. */
    public static final String CUSTOM_TABS_APP_DIRECTORY = "custom_tabs_app";

    /** Prefix for per-app subdirectories under {@link #CUSTOM_TABS_APP_DIRECTORY}. */
    private static final String APP_DIRECTORY_PREFIX = "app_";

    /** Prefix for per-account subdirectories under a per-app directory. */
    private static final String ACCOUNT_DIRECTORY_PREFIX = "acct_";

    /** Default per-account directory suffix when no account ID is provided. */
    private static final String DEFAULT_ACCOUNT_DIRECTORY_SUFFIX = "default";

    /** Prevents two state directories from getting created simultaneously. */
    private static final Object TABBED_MODE_DIR_CREATION_LOCK = new Object();

    /** Prevents two state directories from getting created simultaneously. */
    private static final Object CUSTOM_TABS_DIR_CREATION_LOCK = new Object();

    private static @Nullable File sTabbedModeStateDirectory;
    private static @Nullable File sCustomTabsStateDirectory;

    /**
     * The folder where the state should be saved to.
     * @return A file representing the directory that contains TabModelSelector states.
     */
    public static File getOrCreateTabbedModeStateDirectory() {
        synchronized (TABBED_MODE_DIR_CREATION_LOCK) {
            if (sTabbedModeStateDirectory == null) {
                sTabbedModeStateDirectory =
                        new File(getOrCreateBaseStateDirectory(), TABBED_MODE_DIRECTORY);
                if (!sTabbedModeStateDirectory.exists() && !sTabbedModeStateDirectory.mkdirs()) {
                    Log.e(TAG, "Failed to create state folder: %s", sTabbedModeStateDirectory);
                }
            }
        }
        return sTabbedModeStateDirectory;
    }

    /**
     * The folder where the state should be saved to.
     * @return A file representing the directory that contains TabModelSelector states.
     */
    public static File getOrCreateCustomTabModeStateDirectory() {
        synchronized (CUSTOM_TABS_DIR_CREATION_LOCK) {
            if (sCustomTabsStateDirectory == null) {
                sCustomTabsStateDirectory =
                        new File(getOrCreateBaseStateDirectory(), CUSTOM_TABS_DIRECTORY);
                if (!sCustomTabsStateDirectory.exists() && !sCustomTabsStateDirectory.mkdirs()) {
                    Log.e(TAG, "Failed to create state folder: %s", sCustomTabsStateDirectory);
                }
            }
        }
        return sCustomTabsStateDirectory;
    }

    /**
     * Returns the folder where per-app, per-account Custom Tab state files are saved: {@code
     * <base>/custom_tabs_app/app_<packageName>/acct_<sha256_16(accountId)>} (or {@code
     * acct_default} if {@code accountId} is null or empty).
     *
     * @param packageName The verified package name of the client app.
     * @param accountId The raw account ID string from the launch intent, or null if not set.
     * @return A file representing the per-account state directory.
     */
    public static File getOrCreateCustomTabAppStateDirectory(
            String packageName, @Nullable String accountId) {
        String accountSuffix =
                TextUtils.isEmpty(accountId)
                        ? DEFAULT_ACCOUNT_DIRECTORY_SUFFIX
                        : hashAccountId(accountId);
        return getOrCreateCustomTabAppStateDirectoryForHashedAccount(packageName, accountSuffix);
    }

    /**
     * Returns the folder where per-app, per-account Custom Tab state files are saved given an
     * already-hashed account ID (16-char hex string).
     *
     * @param packageName The verified package name of the client app.
     * @param hashedAccountId The 16-character SHA-256 hex prefix of the account ID.
     * @return A file representing the per-account state directory.
     */
    public static File getOrCreateCustomTabAppStateDirectoryForHashedAccount(
            String packageName, String hashedAccountId) {
        synchronized (CUSTOM_TABS_DIR_CREATION_LOCK) {
            File appRootDir = new File(getOrCreateBaseStateDirectory(), CUSTOM_TABS_APP_DIRECTORY);
            File appDir = new File(appRootDir, APP_DIRECTORY_PREFIX + packageName);
            File accountDir = new File(appDir, ACCOUNT_DIRECTORY_PREFIX + hashedAccountId);
            if (!accountDir.exists() && !accountDir.mkdirs()) {
                Log.e(TAG, "Failed to create state folder: %s", accountDir);
            }
            return accountDir;
        }
    }

    /**
     * Computes the 16-character lowercase hex prefix of the SHA-256 hash of {@code accountId}.
     *
     * @param accountId The account ID to hash.
     * @return A 16-character lowercase hex string.
     */
    public static String hashAccountId(String accountId) {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            byte[] hash = digest.digest(accountId.getBytes(StandardCharsets.UTF_8));
            StringBuilder sb = new StringBuilder(16);
            for (int i = 0; i < 8; i++) {
                sb.append(String.format(Locale.US, "%02x", hash[i] & 0xFF));
            }
            return sb.toString();
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }
    }

    private static class BaseStateDirectoryHolder {
        // Not final for tests.
        private static File sDirectory;

        static {
            sDirectory =
                    ContextUtils.getApplicationContext()
                            .getDir(BASE_STATE_FOLDER, Context.MODE_PRIVATE);
        }
    }

    /**
     * Directory containing all data for TabModels.  Each subdirectory stores info about different
     * TabModelSelectors, including metadata about each TabModel and TabStates for each of their
     * tabs.
     *
     * @return The parent state directory.
     */
    public static File getOrCreateBaseStateDirectory() {
        return BaseStateDirectoryHolder.sDirectory;
    }

    /** Sets where the base state directory is in tests. */
    public static void setBaseStateDirectoryForTests(File directory) {
        var oldValue = BaseStateDirectoryHolder.sDirectory;
        BaseStateDirectoryHolder.sDirectory = directory;
        ResettersForTesting.register(() -> BaseStateDirectoryHolder.sDirectory = oldValue);
    }

    public static void resetTabbedModeStateDirectoryForTesting() {
        sTabbedModeStateDirectory = null;
    }
}
