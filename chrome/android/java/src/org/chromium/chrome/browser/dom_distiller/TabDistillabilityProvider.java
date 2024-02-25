// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.UserData;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.dom_distiller.content.DistillablePageUtils;
import org.chromium.components.dom_distiller.content.DistillablePageUtils.PageDistillableDelegate;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A mechanism for clients interested in the distillability of a page to receive updates. */
public class TabDistillabilityProvider extends EmptyTabObserver
        implements PageDistillableDelegate, UserData {
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
    };

    /** An observer of the distillable state of a tab and its active web content. */
    public interface DistillabilityObserver {
        /**
         * Called when the distillability status changes.
         * @param tab The tab the event was triggered for.
         * @param isDistillable Whether the page is distillable.
         * @param isLast Whether the update is the last one for this page.
         * @param isMobileOptimized Whether the page is optimized for mobile. Only valid when
         *                          the heuristics is ADABOOST_MODEL or ALL_ARTICLES.
         */
        void onIsPageDistillableResult(
                Tab tab, boolean isDistillable, boolean isLast, boolean isMobileOptimized);
    }

    /** The list of observers to propagate events to. */
    private final ObserverList<DistillabilityObserver> mObserverList;

    /** The tab this provider represents. */
    private Tab mTab;

    /** Whether the distillability has been determined for the tab in its current state. */
    private boolean mDistillabilityDetermined;

    /** The last web contents that the distillability delegate was attached to. */
    private WebContents mWebContents;

    /** Cached results from the last result from native. */
    private boolean mIsDistillable;

    private boolean mIsLast;
    private boolean mIsLongArticle;
    private boolean mIsMobileOptimized;

    public static void createForTab(Tab tab) {
        assert get(tab) == null;
        tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabDistillabilityProvider(tab));
    }

    public static @Nullable TabDistillabilityProvider get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private TabDistillabilityProvider(Tab tab) {
        mTab = tab;
        mObserverList = new ObserverList<>();
        resetState();
        mTab.addObserver(this);
    }

    /**
     * Add an observer of distillability updates for this helper.
     * @param observer The observer to add.
     */
    public void addObserver(DistillabilityObserver observer) {
        mObserverList.addObserver(observer);
    }

    /**
     * Remove an observer of distillability updates for this helper.
     * @param observer The observer to remove.
     */
    public void removeObserver(DistillabilityObserver observer) {
        mObserverList.removeObserver(observer);
    }

    /**
     * @return Whether the web content has provided a signal about disillability for the current
     *         page.
     */
    public boolean isDistillabilityDetermined() {
        return mDistillabilityDetermined;
    }

    /** @return Whether the current page is considered distillable. */
    public boolean isDistillable() {
        return mIsDistillable;
    }

    /** @return Whether the last signal has been received from the web content. */
    public boolean isLast() {
        return mIsLast;
    }

    /** @return Whether the current page is considered to be mobile optimized. */
    public boolean isMobileOptimized() {
        return mIsMobileOptimized;
    }

    /**
     * Reset any of the cached values from native distiller and reattach the delegate if necessary.
     */
    private void resetState() {
        mDistillabilityDetermined = false;
        mIsDistillable = false;
        mIsLast = false;
        mIsLongArticle = false;
        mIsMobileOptimized = false;

        if (mTab != null
                && mTab.getWebContents() != null
                && mTab.getWebContents() != mWebContents) {
            mWebContents = mTab.getWebContents();
            DistillablePageUtils.setDelegate(mWebContents, this);
        }
    }

    /** Records the Content.Classification metric if the distillability has been determined. */
    private void recordContentClassificationMetric() {
        // If the distillability was determined, record the Content Classification. Should be called
        // before #resetState().
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
            boolean isDistillable,
            boolean isLast,
            boolean isLongArticle,
            boolean isMobileOptimized) {
        mIsDistillable = isDistillable;
        mIsLast = isLast;
        mIsLongArticle = isLongArticle;
        mIsMobileOptimized = isMobileOptimized;

        mDistillabilityDetermined = true;

        for (DistillabilityObserver o : mObserverList) {
            o.onIsPageDistillableResult(mTab, mIsDistillable, mIsLast, mIsMobileOptimized);
        }
    }

    @Override
    public void onContentChanged(Tab tab) {
        recordContentClassificationMetric();
        resetState();
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        if (window != null) return;
        resetState();
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
        recordContentClassificationMetric();
        resetState();
    }

    @Override
    public void destroy() {
        mObserverList.clear();
        mTab.removeObserver(this);
        mTab = null;
        mWebContents = null;
        resetState();
    }
}
