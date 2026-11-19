// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.reader_mode;

import androidx.annotation.IntDef;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabWebContentsUserData;
import org.chromium.components.web_distiller.content.DistillablePageUtils;
import org.chromium.components.web_distiller.content.DistillablePageUtils.PageDistillableDelegate;
import org.chromium.components.web_distiller.core.WebDistillerUrlUtils;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A mechanism for clients interested in the distillability of a page to receive updates. */
@NullMarked
public class TabDistillabilityProvider extends TabWebContentsUserData
        implements PageDistillableDelegate {
    public static final Class<TabDistillabilityProvider> USER_DATA_KEY =
            TabDistillabilityProvider.class;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        ContentClassification.OTHER,
        ContentClassification.LONG_ARTICLE,
        ContentClassification.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContentClassification {
        int OTHER = 0;
        int LONG_ARTICLE = 1;
        int COUNT = 2;
    }

    /** An observer of the distillable state of a tab and its active web content. */
    public interface DistillabilityObserver {
        /**
         * Called when the distillability status changes.
         *
         * @param tab The tab the event was triggered for.
         * @param isDistillable Whether the page is distillable.
         * @param isLast Whether the update is the last one for this page.
         * @param isMobileOptimized Whether the page is optimized for mobile. Only valid when the
         *     heuristics is ADABOOST_MODEL or ALL_ARTICLES.
         */
        void onIsPageDistillableResult(
                Tab tab, boolean isDistillable, boolean isLast, boolean isMobileOptimized);
    }

    /** The list of observers to propagate events to. */
    private final ObserverList<DistillabilityObserver> mObserverList;

    /** The tab this provider represents. */
    private final Tab mTab;

    /** Whether the distillability has been determined for the tab in its current state. */
    private boolean mDistillabilityDetermined;

    /** Cached results from the last result from native. */
    private boolean mIsDistillable;

    /** Track the last result URL. Used to avoid resetting distillability data on the same page. */
    private @Nullable GURL mDistillationResultUrl;

    private boolean mIsLast;
    private boolean mIsLongArticle;
    private boolean mIsMobileOptimized;

    /**
     * Retrieves the {@link TabDistillabilityProvider} for the given {@link Tab}, creating it if it
     * doesn't already exist.
     *
     * @param tab The Tab to get the helper for.
     * @return The {@link TabDistillabilityProvider}.
     */
    public static TabDistillabilityProvider from(Tab tab) {
        assert !tab.isDestroyed();
        TabDistillabilityProvider provider = get(tab);
        if (provider == null) {
            provider = attachToTab(USER_DATA_KEY, new TabDistillabilityProvider(tab));
        }
        return provider;
    }

    /** Returns the TabDistillabilityProvider for the given tab if it exists. */
    public static @Nullable TabDistillabilityProvider get(Tab tab) {
        if (tab.isDestroyed()) return null;
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private TabDistillabilityProvider(Tab tab) {
        super(tab);
        mTab = tab;
        mObserverList = new ObserverList<>();
        resetDistillabilityState();
    }

    /**
     * Add an observer of distillability updates for this helper.
     *
     * @param observer The observer to add.
     */
    public void addObserver(DistillabilityObserver observer) {
        mObserverList.addObserver(observer);
    }

    /**
     * Remove an observer of distillability updates for this helper.
     *
     * @param observer The observer to remove.
     */
    public void removeObserver(DistillabilityObserver observer) {
        mObserverList.removeObserver(observer);
    }

    /**
     * @return Whether the web content has provided a signal about disillability for the current
     *     page.
     */
    public boolean isDistillabilityDetermined() {
        return mDistillabilityDetermined;
    }

    /**
     * @return Whether the current page is considered distillable.
     */
    public boolean isDistillable() {
        return mIsDistillable;
    }

    /**
     * @return Whether the last signal has been received from the web content.
     */
    public boolean isLast() {
        return mIsLast;
    }

    /**
     * @return Whether the current page is considered to be mobile optimized.
     */
    public boolean isMobileOptimized() {
        return mIsMobileOptimized;
    }

    /** Resets any of the cached values from native distiller. */
    private void resetDistillabilityState() {
        mDistillabilityDetermined = false;
        mDistillationResultUrl = null;
        mIsDistillable = false;
        mIsLast = false;
        mIsLongArticle = false;
        mIsMobileOptimized = false;
    }

    /** Records the Content.Classification metric if the distillability has been determined. */
    private void recordContentClassificationMetric() {
        // If the distillability was determined, record the Content Classification. Should be called
        // before #resetDistillabilityState().
        if (isDistillabilityDetermined()) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Content.Classification",
                    mIsLongArticle
                            ? ContentClassification.LONG_ARTICLE
                            : ContentClassification.OTHER,
                    ContentClassification.COUNT);
        }
    }

    @Override
    public void onIsPageDistillableResult(
            GURL url,
            boolean isDistillable,
            boolean isLast,
            boolean isLongArticle,
            boolean isMobileOptimized) {
        mDistillationResultUrl = url;
        mIsDistillable = isDistillable;
        mIsLast = isLast;
        mIsLongArticle = isLongArticle;
        mIsMobileOptimized = isMobileOptimized;

        mDistillabilityDetermined = true;

        for (DistillabilityObserver o : mObserverList) {
            o.onIsPageDistillableResult(mTab, mIsDistillable, mIsLast, mIsMobileOptimized);
        }
        recordContentClassificationMetric();
    }

    @Override
    public void initWebContents(WebContents webContents) {
        resetDistillabilityState();
        DistillablePageUtils.setDelegate(webContents, this);
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {
        DistillablePageUtils.setDelegate(webContents, /* delegate= */ null);
        resetDistillabilityState();
    }

    @Override
    protected void destroyInternal() {
        mObserverList.clear();
        resetDistillabilityState();
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        if (window == null) {
            resetDistillabilityState();
        }
    }

    @Override
    public void onDidStartNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
        resetStateIfUrlHasChanged(tab);
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
        resetStateIfUrlHasChanged(tab);
    }

    private void resetStateIfUrlHasChanged(Tab tab) {
        // The distillation result remains fresh if navigating within the same article or entering
        // its reader mode view.
        if (mDistillationResultUrl != null) {
            if (mDistillationResultUrl.equalsIgnoringRef(tab.getUrl())) {
                return;
            } else if (WebDistillerUrlUtils.isDistilledPage(tab.getUrl())) {
                GURL originalUrl =
                        WebDistillerUrlUtils.getOriginalUrlFromDistillerUrl(tab.getUrl());
                if (originalUrl != null && mDistillationResultUrl.equalsIgnoringRef(originalUrl)) {
                    return;
                }
            }
        }
        resetDistillabilityState();
    }
}
