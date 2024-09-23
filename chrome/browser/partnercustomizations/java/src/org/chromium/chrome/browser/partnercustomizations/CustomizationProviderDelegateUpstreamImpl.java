// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.content.pm.ApplicationInfo;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.SystemClock;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Delegate used to retrieve information from the ContentProvider about partner customization. */
public class CustomizationProviderDelegateUpstreamImpl implements CustomizationProviderDelegate {
    private static final String PARTNER_DISABLE_BOOKMARKS_EDITING_PATH = "disablebookmarksediting";
    private static final String PARTNER_DISABLE_INCOGNITO_MODE_PATH = "disableincognitomode";
    private static final String TAG = "CustomizationProv";
    private static final String PROVIDER_AUTHORITY = "com.android.partnerbrowsercustomizations";
    private static final String PARTNER_HOMEPAGE_PATH = "homepage";

    private static String sProviderAuthority = PROVIDER_AUTHORITY;
    private static Boolean sIgnoreSystemPackageCheckForTesting;
    private static Boolean sValid;

    /** Provides a way to do some post-process timing for the validation function. */
    interface DelegateValidationCompletion {
        /**
         * When validation has completed, notify the closure of how long that took, regardless of
         * outcome.
         */
        void validated(long startTime);
    }

    /**
     * A completion to call after determining isValid that includes timing information. Typically
     * {@code null} on Chromium but can be set from Downstream.
     */
    @Nullable private DelegateValidationCompletion mValidationCompletion;

    @Override
    public @Nullable String getHomepage() {
        if (!isValid()) {
            return null;
        }
        String homepage = null;
        Cursor cursor =
                ContextUtils.getApplicationContext()
                        .getContentResolver()
                        .query(buildQueryUri(PARTNER_HOMEPAGE_PATH), null, null, null, null);
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
        Cursor cursor =
                ContextUtils.getApplicationContext()
                        .getContentResolver()
                        .query(
                                buildQueryUri(PARTNER_DISABLE_INCOGNITO_MODE_PATH),
                                null,
                                null,
                                null,
                                null);
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
        Cursor cursor =
                ContextUtils.getApplicationContext()
                        .getContentResolver()
                        .query(
                                buildQueryUri(PARTNER_DISABLE_BOOKMARKS_EDITING_PATH),
                                null,
                                null,
                                null,
                                null);
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
                ContextUtils.getApplicationContext()
                        .getPackageManager()
                        .resolveContentProvider(sProviderAuthority, 0);
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
        if (sIgnoreSystemPackageCheckForTesting != null && sIgnoreSystemPackageCheckForTesting) {
            return true;
        }

        Log.w(
                TAG,
                "Browser Customizations content provider package, "
                        + providerInfo.packageName
                        + ", is not a system package. "
                        + "This could be a malicious attempt from a third party "
                        + "app, so skip reading the browser content provider.");
        if (sIgnoreSystemPackageCheckForTesting != null && !sIgnoreSystemPackageCheckForTesting) {
            return false;
        }
        if (VersionInfo.isLocalBuild()) {
            Log.w(
                    TAG,
                    "This is a local build of Chrome Android, "
                            + "so keep reading the browser content provider, "
                            + "to make debugging customization easier.");
            return true;
        }
        // ProviderInfo was present, but flags don't indicate it was a System APK (above), and none
        // of our overrides apply.
        return false;
    }

    /**
     * May be called by Downstream to determine if the default Upstream delegate is actually being
     * used.
     */
    boolean isValid() {
        if (sValid == null) {
            long validationStartTime = SystemClock.elapsedRealtime();
            sValid = isValidInternal();
            if (mValidationCompletion != null) {
                mValidationCompletion.validated(validationStartTime);
            }
        }
        return sValid;
    }

    /** Sets a function to call when validation has been performed. */
    void setValidationCompletion(DelegateValidationCompletion validationCompletion) {
        assert mValidationCompletion == null
                : "Coding error: setValidationCompletion may only be called once!";
        mValidationCompletion = validationCompletion;
    }

    static Uri buildQueryUri(String path) {
        return new Uri.Builder()
                .scheme(UrlConstants.CONTENT_SCHEME)
                .authority(sProviderAuthority)
                .appendPath(path)
                .build();
    }

    static void setProviderAuthorityForTesting(String providerAuthority) {
        var oldValue = sProviderAuthority;
        sProviderAuthority = providerAuthority;
        ResettersForTesting.register(() -> sProviderAuthority = oldValue);
    }

    /**
     * For security, we only allow system package to be a browser customizations provider. However,
     * requiring root and installing system apk makes testing harder, so we decided to have this
     * hack for testing. This must not be called other than tests.
     *
     * @param ignore whether we should ignore browser provider system package checking.
     */
    static void ignoreBrowserProviderSystemPackageCheckForTesting(boolean ignore) {
        sIgnoreSystemPackageCheckForTesting = ignore;
    }
}
