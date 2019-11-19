// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.content.Intent;
import android.content.pm.ResolveInfo;

import androidx.annotation.NonNull;

import org.chromium.base.PackageManagerUtils;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.WebappScopePolicy;

import java.util.List;

/**
 * A delegate for the class responsible for navigating to external applications from Chrome. Used
 * by {@link ExternalNavigationHandler}.
 */
interface ExternalNavigationDelegate {
    /**
     * See {@link PackageManagerUtils#queryIntentActivities(Intent, int)}
     */
    @NonNull
    List<ResolveInfo> queryIntentActivities(Intent intent);

    /**
     * Determine if Chrome is the default or only handler for a given intent. If true, Chrome
     * will handle the intent when started.
     */
    boolean willChromeHandleIntent(Intent intent);

    /**
     * If the current activity is a webapp, applies the webapp's scope policy and returns the
     * result. Returns {@link WebappScopePolicy#NavigationDirective#NORMAL_BEHAVIOR} if the current
     * activity is not a webapp.
     */
    @WebappScopePolicy.NavigationDirective
    int applyWebappScopePolicyForUrl(String url);

    /**
     * Returns the number of specialized intent handlers in {@params infos}. Specialized intent
     * handlers are intent handlers which handle only a few URLs (e.g. google maps or youtube).
     */
    int countSpecializedHandlers(List<ResolveInfo> infos);

    /**
     * Start an activity for the intent. Used for intents that must be handled externally.
     * @param intent The intent we want to send.
     * @param proxy Whether we need to proxy the intent through AuthenticatedProxyActivity (this is
     *              used by Instant Apps intents).
     */
    void startActivity(Intent intent, boolean proxy);

    /**
     * Start an activity for the intent. Used for intents that may be handled internally or
     * externally. If the user chooses to handle the intent internally, this routine must return
     * false.
     * @param intent The intent we want to send.
     * @param proxy Whether we need to proxy the intent through AuthenticatedProxyActivity (this is
     *              used by Instant Apps intents).
     */
    boolean startActivityIfNeeded(Intent intent, boolean proxy);

    /**
     * Display a dialog warning the user that they may be leaving Chrome by starting this
     * intent. Give the user the opportunity to cancel the action. And if it is canceled, a
     * navigation will happen in Chrome. Catches BadTokenExceptions caused by showing the dialog
     * on certain devices. (crbug.com/782602)
     * @param intent The intent for external application that will be sent.
     * @param referrerUrl The referrer for the current navigation.
     * @param fallbackUrl The URL to load if the user doesn't proceed with external intent.
     * @param tab The current tab.
     * @param needsToCloseTab Whether the current tab has to be closed after the intent is sent.
     * @param proxy Whether we need to proxy the intent through AuthenticatedProxyActivity (this is
     *              used by Instant Apps intents.
     * @return True if the function returned error free, false if it threw an exception.
     */
    boolean startIncognitoIntent(Intent intent, String referrerUrl, String fallbackUrl, Tab tab,
            boolean needsToCloseTab, boolean proxy);

    /**
     * @param url The requested url.
     * @return Whether we should block the navigation and request file access before proceeding.
     */
    boolean shouldRequestFileAccess(String url);

    /**
     * Trigger a UI affordance that will ask the user to grant file access.  After the access
     * has been granted or denied, continue loading the specified file URL.
     *
     * @param intent The intent to continue loading the file URL.
     * @param referrerUrl The HTTP referrer URL.
     * @param needsToCloseTab Whether this action should close the current tab.
     */
    void startFileIntent(Intent intent, String referrerUrl, boolean needsToCloseTab);

    /**
     * Clobber the current tab and try not to pass an intent when it should be handled by Chrome
     * so that we can deliver HTTP referrer information safely.
     *
     * @param url The new URL after clobbering the current tab.
     * @param referrerUrl The HTTP referrer URL.
     * @return OverrideUrlLoadingResult (if the tab has been clobbered, or we're launching an
     *         intent.)
     */
    @OverrideUrlLoadingResult
    int clobberCurrentTab(String url, String referrerUrl);

    /** Adds a window id to the intent, if necessary. */
    void maybeSetWindowId(Intent intent);

    /** Adds the package name of a specialized intent handler. */
    void maybeRecordAppHandlersInIntent(Intent intent, List<ResolveInfo> info);

    /**
     * Determine if the Chrome app is in the foreground.
     */
    boolean isChromeAppInForeground();

    /**
     * @return Default SMS application's package name. Null if there isn't any.
     */
    String getDefaultSmsPackageName();

    /**
     * @return Whether the URL is a file download.
     */
    boolean isPdfDownload(String url);

    /**
     * Check if the URL should be handled by an instant app, or kick off an async request for an
     * instant app banner.
     * @param url The current URL.
     * @param referrerUrl The referrer URL.
     * @param isIncomingRedirect Whether we are handling an incoming redirect to an instant app.
     * @return Whether we launched an instant app.
     */
    boolean maybeLaunchInstantApp(String url, String referrerUrl, boolean isIncomingRedirect);

    /**
     * @return whether this navigation is from the search results page.
     */
    boolean isSerpReferrer();

    /**
     * @return The previously committed URL from the WebContents.
     */
    String getPreviousUrl();

    /**
     * @param intent The intent to launch.
     * @return Whether the Intent points to an app that we trust and that launched Chrome.
     */
    boolean isIntentForTrustedCallingApp(Intent intent);

    /**
     * @param packageName The package to check.
     * @return Whether the package is a valid WebAPK package.
     */
    boolean isValidWebApk(String packageName);

    /**
     * @return Whether the current tab is custom tab or not.
     */
    default boolean isOnCustomTab() {
        return false;
    }
}
