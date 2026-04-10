// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.AllTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.HashSet;
import java.util.Set;

/** An observer that filters tabs and forwards events to a delegate. */
@NullMarked
public class MediaCapturePickerTabObserver implements AllTabObserver.Observer {
    /** A delegate to handle filtering of tabs. */
    public interface FilterDelegate {
        /**
         * Called to check if a tab should be filtered.
         *
         * @param webContents The contents to check.
         * @return True if the tab should be filtered.
         */
        boolean shouldFilterWebContents(WebContents webContents);
    }

    /** A delegate to receive events about tabs. */
    public interface Delegate extends AllTabObserver.Observer {
        /**
         * Called when a pickable tab is updated. To be replaced by the fine-grained update methods.
         */
        @Deprecated
        default void onTabUpdated(Tab tab) {}

        /** Called when a pickable tab's title is updated. */
        default void onTabTitleUpdated(Tab tab) {
            onTabUpdated(tab);
        }

        /** Called when a pickable tab's icon is updated. */
        default void onTabIconUpdated(Tab tab, @Nullable Bitmap icon) {
            onTabUpdated(tab);
        }

        /** Called when a pickable tab's content is updated. */
        default void onTabContentUpdated(Tab tab) {
            onTabUpdated(tab);
        }

        @Override
        void onTabAdded(Tab tab);

        @Override
        void onTabRemoved(Tab tab);
    }

    private final Delegate mObserverDelegate;
    private final MediaCapturePickerManager.Params mParams;
    private final FilterDelegate mFilterDelegate;
    private final Set<Tab> mPickableTabs = new HashSet<>();
    private final Set<Tab> mObservedTabs = new HashSet<>();

    private final TabObserver mTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onTitleUpdated(Tab tab) {
                    maybeUpdatePickableTab(tab, mObserverDelegate::onTabTitleUpdated);
                }

                @Override
                public void onFaviconUpdated(
                        Tab tab, @Nullable Bitmap icon, @Nullable GURL iconUrl) {
                    maybeUpdatePickableTab(tab, (t) -> mObserverDelegate.onTabIconUpdated(t, icon));
                }

                @Override
                public void onUrlUpdated(Tab tab) {
                    maybeUpdatePickableTab(tab, mObserverDelegate::onTabContentUpdated);
                }

                @Override
                public void onContentChanged(Tab tab) {
                    maybeUpdatePickableTab(tab, mObserverDelegate::onTabContentUpdated);
                }

                @Override
                public void onPageLoadFinished(Tab tab, GURL url) {
                    maybeUpdatePickableTab(tab, mObserverDelegate::onTabContentUpdated);
                }
            };

    public MediaCapturePickerTabObserver(
            Delegate delegate,
            MediaCapturePickerManager.Params params,
            FilterDelegate filterDelegate) {
        mObserverDelegate = delegate;
        mParams = params;
        mFilterDelegate = filterDelegate;
    }

    public void destroy() {
        for (Tab tab : mObservedTabs) {
            tab.removeObserver(mTabObserver);
        }
        mObservedTabs.clear();
        mPickableTabs.clear();
    }

    private void maybeUpdatePickableTab(Tab tab, Callback<Tab> updateAction) {
        if (isTabPickable(tab)) {
            if (mPickableTabs.add(tab)) {
                mObserverDelegate.onTabAdded(tab);
            } else {
                updateAction.onResult(tab);
            }
        } else {
            if (mPickableTabs.remove(tab)) mObserverDelegate.onTabRemoved(tab);
        }
    }

    private void maybeUpdatePickableTab(Tab tab) {
        maybeUpdatePickableTab(tab, mObserverDelegate::onTabUpdated);
    }

    private boolean isTabPickable(Tab tab) {
        // We do not support capture of native pages.
        if (NativePage.isChromePageUrl(tab.getUrl(), tab.isIncognito())) return false;

        // Filter out all tabs that are not this tab for capture this tab.
        if (mParams.captureThisTab && tab.getWebContents() != mParams.webContents) return false;

        final WebContents webContents = tab.getWebContents();
        if (webContents == null) return false;

        return !mFilterDelegate.shouldFilterWebContents(webContents);
    }

    @Override
    public void onTabAdded(Tab tab) {
        final boolean added = mObservedTabs.add(tab);
        assert added;
        tab.addObserver(mTabObserver);
        maybeUpdatePickableTab(tab);
    }

    @Override
    public void onTabRemoved(Tab tab) {
        final boolean removed = mObservedTabs.remove(tab);
        assert removed;
        tab.removeObserver(mTabObserver);
        if (mPickableTabs.remove(tab)) mObserverDelegate.onTabRemoved(tab);
    }
}
