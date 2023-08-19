// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.SystemClock;
import android.text.format.DateUtils;
import android.util.LruCache;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.DocumentMetadata;
import org.chromium.blink.mojom.WebPage;
import org.chromium.chrome.browser.historyreport.AppIndexingReporter;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This is the top-level CopylessPaste metadata extraction for AppIndexing.
 */
public class AppIndexingUtil {
    private static final int CACHE_SIZE = 100;
    private static final int CACHE_VISIT_CUTOFF_MS = (int) DateUtils.HOUR_IN_MILLIS;
    // Cache of recently seen urls. If a url is among the CACHE_SIZE most recent pages visited, and
    // the parse was in the last CACHE_VISIT_CUTOFF_MS milliseconds, then we don't parse the page,
    // and instead just report the view (not the content) to App Indexing.
    private LruCache<String, CacheEntry> mPageCache;
    private TabModelSelectorTabObserver mObserver;

    private static Callback<WebPage> sCallbackForTesting;

    // Constants used to log UMA "enum" histograms about the cache state.
    // The values should not be changed or reused, and CACHE_HISTOGRAM_BOUNDARY should be the last.
    @IntDef({CacheHit.WITH_ENTITY, CacheHit.WITHOUT_ENTITY, CacheHit.MISS})
    @Retention(RetentionPolicy.SOURCE)
    private @interface CacheHit {
        int WITH_ENTITY = 0;
        int WITHOUT_ENTITY = 1;
        int MISS = 2;
        int NUM_ENTRIES = 3;
    }

    AppIndexingUtil(@Nullable TabModelSelector mTabModelSelectorImpl) {
        if (mTabModelSelectorImpl != null && isEnabledForDevice()) {
            mObserver = new TabModelSelectorTabObserver(mTabModelSelectorImpl) {
                @Override
                public void onPageLoadFinished(final Tab tab, GURL url) {
                    // Part of scroll jank investigation http://crbug.com/1311003. Will remove
                    // TraceEvent after the investigation is complete.
                    try (TraceEvent te = TraceEvent.scoped("AppIndexingUtil::onPageLoadFinished")) {
                        extractDocumentMetadata(tab);
                    }
                }

                @Override
                public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                    reportPageView(tab);
                }
            };
        }
    }

    void destroy() {
        if (mObserver != null) mObserver.destroy();
    }

    /**
     * Extracts entities from document metadata and reports it to on-device App Indexing.
     * This call can cache entities from recently parsed webpages, in which case, only the url and
     * title of the page is reported to App Indexing.
     */
    @VisibleForTesting
    void extractDocumentMetadata(final Tab tab) {
        if (!isEnabledForTab(tab)) return;

        // There are three conditions that can occur with respect to the cache.
        // 1. Cache hit, and an entity was found previously.
        // 2. Cache hit, but no entity was found. Ignore.
        // 3. Cache miss, we need to parse the page.
        // Note that page view is reported unconditionally.
        final String url = tab.getUrl().getSpec();
        if (wasPageVisitedRecently(url)) {
            if (lastPageVisitContainedEntity(url)) {
                // Condition 1
                RecordHistogram.recordEnumeratedHistogram(
                        "CopylessPaste.CacheHit", CacheHit.WITH_ENTITY, CacheHit.NUM_ENTRIES);
                return;
            }
            // Condition 2
            RecordHistogram.recordEnumeratedHistogram(
                    "CopylessPaste.CacheHit", CacheHit.WITHOUT_ENTITY, CacheHit.NUM_ENTRIES);
        } else {
            // Condition 3
            RecordHistogram.recordEnumeratedHistogram(
                    "CopylessPaste.CacheHit", CacheHit.MISS, CacheHit.NUM_ENTRIES);
            DocumentMetadata documentMetadata = getDocumentMetadataInterface(tab);
            if (documentMetadata == null) {
                return;
            }
            documentMetadata.getEntities(webpage -> {
                documentMetadata.close();
                putCacheEntry(url, webpage != null);
                if (sCallbackForTesting != null) {
                    sCallbackForTesting.onResult(webpage);
                }
                if (webpage == null) return;
                getAppIndexingReporter().reportWebPage(webpage);
            });
        }
    }

    @VisibleForTesting
    void reportPageView(Tab tab) {
        if (!isEnabledForTab(tab)) return;
        getAppIndexingReporter().reportWebPageView(tab.getUrl().getSpec(), tab.getTitle());
    }

    static void setCallbackForTesting(Callback<WebPage> callback) {
        sCallbackForTesting = callback;
        ResettersForTesting.register(() -> sCallbackForTesting = null);
    }

    private boolean wasPageVisitedRecently(String url) {
        if (url == null) return false;
        CacheEntry entry = getPageCache().get(url);
        if (entry == null || (getElapsedTime() - entry.lastSeenTimeMs > CACHE_VISIT_CUTOFF_MS)) {
            return false;
        }
        return true;
    }

    /**
     * Returns true if the page is in the cache and it contained an entity the last time it was
     * parsed.
     */
    private boolean lastPageVisitContainedEntity(String url) {
        if (url == null) return false;
        CacheEntry entry = getPageCache().get(url);
        if (entry == null || !entry.containedEntity) {
            return false;
        }
        return true;
    }

    private void putCacheEntry(String url, boolean containedEntity) {
        CacheEntry entry = new CacheEntry();
        entry.lastSeenTimeMs = getElapsedTime();
        entry.containedEntity = containedEntity;
        getPageCache().put(url, entry);
    }

    @VisibleForTesting
    AppIndexingReporter getAppIndexingReporter() {
        return AppIndexingReporter.getInstance();
    }

    @VisibleForTesting
    DocumentMetadata getDocumentMetadataInterface(Tab tab) {
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return null;

        RenderFrameHost mainFrame = webContents.getMainFrame();
        if (mainFrame == null) return null;

        if (!mainFrame.isRenderFrameLive()) return null;

        return mainFrame.getInterfaceToRendererFrame(DocumentMetadata.MANAGER);
    }

    @VisibleForTesting
    long getElapsedTime() {
        return SystemClock.elapsedRealtime();
    }

    @VisibleForTesting
    boolean isEnabledForDevice() {
        return !SysUtils.isLowEndDevice();
    }

    @VisibleForTesting
    boolean isEnabledForTab(Tab tab) {
        boolean isHttpOrHttps = UrlUtilities.isHttpOrHttps(tab.getUrl());
        return isEnabledForDevice() && !tab.isIncognito() && isHttpOrHttps;
    }

    private LruCache<String, CacheEntry> getPageCache() {
        if (mPageCache == null) {
            mPageCache = new LruCache<String, CacheEntry>(CACHE_SIZE);
        }
        return mPageCache;
    }

    private static class CacheEntry {
        public long lastSeenTimeMs;
        public boolean containedEntity;
    }
}
