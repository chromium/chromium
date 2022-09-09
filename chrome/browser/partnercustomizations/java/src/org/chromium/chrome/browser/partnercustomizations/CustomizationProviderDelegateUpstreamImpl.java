// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.content.pm.ApplicationInfo;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.net.Uri;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.version_info.VersionInfo;

/** Delegate used to retrieve information from the ContentProvider about partner customization. */
public class CustomizationProviderDelegateUpstreamImpl implements CustomizationProviderDelegate {
    private static final String PARTNER_DISABLE_BOOKMARKS_EDITING_PATH = "disablebookmarksediting";
    private static final String PARTNER_DISABLE_INCOGNITO_MODE_PATH = "disableincognitomode";
    private static final String TAG = "CustomizationProv";
    private static final String PROVIDER_AUTHORITY = "com.android.partnerbrowsercustomizations";
    private static final String PARTNER_HOMEPAGE_PATH = "homepage";

    private static String sProviderAuthority = PROVIDER_AUTHORITY;
    private static Boolean sIgnoreSystemPackageCheck;
    private static Boolean sValid;

    @Override
    public @Nullable String getHomepage() {
        if (!isValid()) {
            return null;
        }
        String homepage = null;
        Cursor cursor = ContextUtils.getApplicationContext().getContentResolver().query(
                buildQueryUri(PARTNER_HOMEPAGE_PATH), null, null, null, null);
        if (cursor != null && cursor.moveToFirst() && cursor.getColumnCount() == 1) {
            homepage = cursor.getString(0);
        }
        if (cursor != null) {
            cursor.close();
        }
        return homepage;
    }

    @Override
    public boolean isIncognitoModeDisabled() {
        if (!isValid()) {
            return false;
        }
        boolean disabled = false;
        Cursor cursor = ContextUtils.getApplicationContext().getContentResolver().query(
                buildQueryUri(PARTNER_DISABLE_INCOGNITO_MODE_PATH), null, null, null, null);
        if (cursor != null && cursor.moveToFirst() && cursor.getColumnCount() == 1) {
            disabled = cursor.getInt(0) == 1;
        }
        if (cursor != null) {
            cursor.close();
        }
        return disabled;
    }

    @Override
    public boolean isBookmarksEditingDisabled() {
        if (!isValid()) {
            return false;
        }
        boolean disabled = false;
        Cursor cursor = ContextUtils.getApplicationContext().getContentResolver().query(
                buildQueryUri(PARTNER_DISABLE_BOOKMARKS_EDITING_PATH), null, null, null, null);
        if (cursor != null && cursor.moveToFirst() && cursor.getColumnCount() == 1) {
            disabled = cursor.getInt(0) == 1;
        }
        if (cursor != null) {
            cursor.close();
        }
        return disabled;
    }

    private boolean isValidInternal() {
        ProviderInfo providerInfo =
                ContextUtils.getApplicationContext().getPackageManager().resolveContentProvider(
                        sProviderAuthority, 0);
        if (providerInfo == null) {
            return false;
        }
        if ((providerInfo.applicationInfo.flags & ApplicationInfo.FLAG_SYSTEM) != 0) {
            return true;
        }

        // In prod mode (sIgnoreBrowserProviderSystemPackageCheck == null), non-system package
        // is rejected unless Chrome Android is a local build.
        // When sIgnoreBrowserProviderSystemPackageCheck is true, accept non-system package.
        // When sIgnoreBrowserProviderSystemPackageCheck is false, reject non-system package.
        if (sIgnoreSystemPackageCheck != null && sIgnoreSystemPackageCheck) {
            return true;
        }

        Log.w(TAG,
                "Browser Customizations content provider package, " + providerInfo.packageName
                        + ", is not a system package. "
                        + "This could be a malicious attempt from a third party "
                        + "app, so skip reading the browser content provider.");
        if (sIgnoreSystemPackageCheck != null && !sIgnoreSystemPackageCheck) {
            return false;
        }
        if (VersionInfo.isLocalBuild()) {
            Log.w(TAG,
                    "This is a local build of Chrome Android, "
                            + "so keep reading the browser content provider, "
                            + "to make debugging customization easier.");
            return true;
        }
        return false;
    }

    private boolean isValid() {
        if (sValid == null) {
            sValid = isValidInternal();
        }
        return sValid;
    }

    static Uri buildQueryUri(String path) {
        return new Uri.Builder()
                .scheme(UrlConstants.CONTENT_SCHEME)
                .authority(sProviderAuthority)
                .appendPath(path)
                .build();
    }

    @VisibleForTesting
    static void setProviderAuthorityForTesting(String providerAuthority) {
        sProviderAuthority = providerAuthority;
    }

    /**
     * For security, we only allow system package to be a browser customizations provider. However,
     * requiring root and installing system apk makes testing harder, so we decided to have this
     * hack for testing. This must not be called other than tests.
     *
     * @param ignore whether we should ignore browser provider system package checking.
     */
    @VisibleForTesting
    static void ignoreBrowserProviderSystemPackageCheckForTesting(boolean ignore) {
        sIgnoreSystemPackageCheck = ignore;
    }
}