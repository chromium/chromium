// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.net.Uri;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContentUriOwner;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.url.GURL;

import java.util.Locale;

/** Helper for determining if a URL is safe to open from an external intent. */
@JNINamespace("chrome::android")
@NullMarked
public class ExternalIntentUrlChecker {
    /**
     * Determines whether the given GURL is unsafe to open from an external intent.
     *
     * @param url The URL to check.
     * @return Whether the URL is unsafe.
     */
    public static boolean isUnsafeExternalIntentUrl(@Nullable GURL url) {
        return isUnsafeExternalIntentUrl(url, /* allowLocalFiles= */ true);
    }

    /**
     * Determines whether the given GURL is unsafe to open from an external intent.
     *
     * @param url The URL to check.
     * @param allowLocalFiles Whether to allow file:// and content:// schemes.
     * @return Whether the URL is unsafe.
     */
    public static boolean isUnsafeExternalIntentUrl(@Nullable GURL url, boolean allowLocalFiles) {
        if (url == null || !url.isValid() || url.isEmpty()) return true;

        String scheme = url.getScheme();
        if (isUnsafeExternalScheme(scheme)) return true;

        if (!allowLocalFiles
                && (UrlConstants.FILE_SCHEME.equals(scheme)
                        || UrlConstants.CONTENT_SCHEME.equals(scheme))) {
            return true;
        }

        String urlString = url.getSpec();

        // Block external intents that target Chrome's own content providers (e.g., FileProvider).
        // Untrusted apps should not be able to force Chrome to open its own protected files,
        // which could lead to Confused Deputy vulnerabilities.
        if (UrlConstants.CONTENT_SCHEME.equals(scheme)) {
            Uri uri = Uri.parse(urlString);
            // Only block NON_EXPORTED_FROM_CURRENT_APP here (matching
            // IsContentSchemeURLNavigationAllowed in chrome_content_browser_client_android.cc)
            // so exported test providers (e.g. TestContentProvider) remain reachable in
            // instrumentation tests. Production Chrome providers (including exported ones) all
            // use Chrome's package name prefix and are covered by the authority check below.
            @ContentUriOwner int owner = ContentUriUtils.resolveContentUriOwner(uri);
            if (owner == ContentUriOwner.NON_EXPORTED_FROM_CURRENT_APP) {
                return true;
            }
            String authority = uri.getAuthority();
            if (authority != null && !authority.isEmpty()) {
                String lowerAuthority = authority.toLowerCase(Locale.US);
                int atIndex = lowerAuthority.lastIndexOf('@');
                if (atIndex >= 0) {
                    lowerAuthority = lowerAuthority.substring(atIndex + 1);
                }
                String packageName =
                        ContextUtils.getApplicationContext()
                                .getPackageName()
                                .toLowerCase(Locale.US);
                if (lowerAuthority.equals(packageName)
                        || lowerAuthority.startsWith(packageName + ".")) {
                    return true;
                }
            }
        }

        if (!ExternalIntentUrlCheckerJni.get().validateUrl(url)) {
            // Check for safe internal exceptions.
            // Note: GURL spec is canonicalized and scheme/host are lowercase.
            if (urlString.equals(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL)
                    || urlString.equals(ContentUrlConstants.ABOUT_BLANK_URL)
                    || urlString.equals(UrlConstants.CHROME_DINO_URL)
                    || urlString.startsWith(UrlConstants.CHROME_EXTENSIONS_URL)) {
                return false;
            }
            return true;
        }
        return false;
    }

    /**
     * Determines whether the given scheme is unsafe to open from an external intent (e.g.
     * javascript: or jar:). This methods rejects {@link #GOOGLECHROME_SCHEME}, so input scheme need
     * to be unwrapped to prevent malicious schemes from bypassing this check.
     *
     * @param scheme The scheme to check.
     * @return Whether the scheme is unsafe.
     */
    public static boolean isUnsafeExternalScheme(@Nullable String scheme) {
        if (scheme == null || scheme.isEmpty()) return false;
        String lowerCaseScheme = scheme.toLowerCase(Locale.US);
        return lowerCaseScheme.equals(UrlConstants.JAVASCRIPT_SCHEME)
                || lowerCaseScheme.equals(UrlConstants.JAR_SCHEME)
                || lowerCaseScheme.equals(IntentHandler.GOOGLECHROME_SCHEME);
    }

    @NativeMethods
    public interface Natives {
        boolean validateUrl(GURL url);
    }
}
