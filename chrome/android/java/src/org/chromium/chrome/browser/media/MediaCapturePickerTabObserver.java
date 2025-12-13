// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.tabmodel.AllTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;

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
        /** Called when a pickable tab is updated. */
        void onTabUpdated(Tab tab);

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
                public void onContentChanged(Tab tab) {
                    maybeUpdatePickableTab(tab);
                }

                @Override
                public void onTitleUpdated(Tab tab) {
                    maybeUpdatePickableTab(tab);
                }

                @Override
                public void onUrlUpdated(Tab tab) {
                    maybeUpdatePickableTab(tab);
                }

                @Override
                public void onDidFinishNavigationInPrimaryMainFrame(
                        Tab tab, NavigationHandle navigation) {
                    maybeUpdatePickableTab(tab);
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

    private void maybeUpdatePickableTab(Tab tab) {
        if (isTabPickable(tab)) {
            if (mPickableTabs.add(tab)) {
                mObserverDelegate.onTabAdded(tab);
            } else {
                mObserverDelegate.onTabUpdated(tab);
            }
        } else {
            if (mPickableTabs.remove(tab)) mObserverDelegate.onTabRemoved(tab);
        }
    }

    private boolean isTabPickable(Tab tab) {
        // We do not support capture of native pages.
        if (tab.isNativePage()) return false;

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
