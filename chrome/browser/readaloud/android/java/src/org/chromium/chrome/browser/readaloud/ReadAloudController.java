// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelTabObserver;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;

/**
 * The main entrypoint component for Read Aloud feature. It's responsible for
 * checking its availability and triggering playback.
 **/
public class ReadAloudController {
    private static final String TAG = "ReadAloudController";

    private final ObservableSupplier<Profile> mProfileSupplier;
    private final Map<String, Boolean> mReadabilityMap = new HashMap<>();
    private final Map<String, Boolean> mTimepointsSupportedMap = new HashMap<>();
    private final HashSet<String> mPendingRequests = new HashSet<>();
    private final TabModel mTabModel;
    private TabModelTabObserver mTabObserver;

    private final ReadAloudReadabilityHooks mReadabilityHooks;
    @Nullable
    private static ReadAloudReadabilityHooks sReadabilityHooksForTesting;

    /**
     * Kicks of readability check on a page load iff: the url is valid, no previous
     * result is available/pending and if a request has to be sent, the necessary
     * conditions are satisfied.
     * TODO: Add optimizations (don't send requests on chrome:// pages, remove
     * password from the url, etc). Also include enterprise policy check.
     */

    private ReadAloudReadabilityHooks.ReadabilityCallback mReadabilityCallback =
            new ReadAloudReadabilityHooks.ReadabilityCallback() {
                @Override
                public void onSuccess(String url, boolean isReadable, boolean timepointsSupported) {
                    Log.i(TAG, "onSuccess called for %s", url);
                    mReadabilityMap.put(url, isReadable);
                    mTimepointsSupportedMap.put(url, timepointsSupported);
                    mPendingRequests.remove(url);
                }

                @Override
                public void onFailure(String url, Throwable t) {
                    Log.i(TAG, "onFailure called for %s", url);
                    mPendingRequests.remove(url);
                }
            };

    public ReadAloudController(
            Context context, ObservableSupplier<Profile> profileSupplier, TabModel tabModel) {
        mProfileSupplier = profileSupplier;
        mTabModel = tabModel;
        mReadabilityHooks = sReadabilityHooksForTesting != null
                ? sReadabilityHooksForTesting
                : new ReadAloudReadabilityHooksImpl(context, /* apiKeyOverride= */ null);
        if (mReadabilityHooks.isEnabled()) {
            mTabObserver = new TabModelTabObserver(mTabModel) {
                @Override
                public void onPageLoadStarted(Tab tab, GURL url) {
                    Log.i(TAG, "onPageLoad called for %s", url.getPossiblyInvalidSpec());
                    if (!isURLReadAloudSupported(url)) {
                        return;
                    }
                    String urlSpec = url.getSpec();
                    if (mReadabilityMap.containsKey(urlSpec)
                            || mPendingRequests.contains(urlSpec)) {
                        return;
                    }

                    if (!isAvailable()) {
                        return;
                    }

                    mPendingRequests.add(urlSpec);
                    mReadabilityHooks.isPageReadable(urlSpec, mReadabilityCallback);
                }
            };
        }
    }

    /**
     * Checks if URL is supported by Read Aloud before sending a readability request.
     * Read Aloud won't be supported on the following URLs:
     * - pages without an HTTP(S) scheme
     * - myaccount.google.com and myactivity.google.com
     * - www.google.com/...
     *   - Based on standards.google/processes/domains/domain-guidelines-by-use-case,
     *     www.google.com/... is reserved for Search features and content.
     */
    public boolean isURLReadAloudSupported(GURL url) {
        return url.isValid() && !url.isEmpty()
                && (url.getScheme().equals(UrlConstants.HTTP_SCHEME)
                        || url.getScheme().equals(UrlConstants.HTTPS_SCHEME))
                && !url.getSpec().startsWith(UrlConstants.GOOGLE_ACCOUNT_HOME_URL)
                && !url.getSpec().startsWith(UrlConstants.MY_ACTIVITY_HOME_URL)
                && !url.getSpec().startsWith(UrlConstants.GOOGLE_URL);
    }

    /**
     * Checks if Read Aloud is supported which is true iff: user is not in the
     * incognito mode and user opted into "Make searches and browsing better".
     */
    public boolean isAvailable() {
        Profile profile = mProfileSupplier.get();
        if (profile == null || profile.isOffTheRecord()) {
            return false;
        }
        // Check whether the user has enabled anonymous URL-keyed data collection.
        // This is surfaced on the relatively new "Make searches and browsing better"
        // user setting.
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(profile);
    }

    /**
     * Returns true if the web contents within current Tab is readable.
     */
    public boolean isReadable(Tab tab) {
        if (isAvailable() && tab.getUrl().isValid()) {
            Boolean isReadable = mReadabilityMap.get(tab.getUrl().getSpec());
            return isReadable == null ? false : isReadable;
        }
        return false;
    }

    public void playTab(Tab tab) {
        Log.e(TAG, "playTab() not implemented.");
    }

    /**
     * Whether or not timepoints are supported for the tab's content.
     * Timepoints are needed for word highlighting.
     */
    public boolean timepointsSupported(Tab tab) {
        if (isAvailable() && tab.getUrl().isValid()) {
            Boolean timepointsSuported = mTimepointsSupportedMap.get(tab.getUrl().getSpec());
            return timepointsSuported == null ? false : timepointsSuported;
        }
        return false;
    }

    /** Cleanup: unregister listeners. */
    public void destroy() {
        if (mTabObserver != null) {
            mTabObserver.destroy();
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void setReadabilityHooks(ReadAloudReadabilityHooks hooks) {
        sReadabilityHooksForTesting = hooks;
        ResettersForTesting.register(() -> sReadabilityHooksForTesting = null);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public TabModelTabObserver getTabModelTabObserver() {
        return mTabObserver;
    }
}
