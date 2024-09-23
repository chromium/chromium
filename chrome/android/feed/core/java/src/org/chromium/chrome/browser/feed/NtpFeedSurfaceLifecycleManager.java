// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.url.GURL;

/**
 * Manages the lifecycle of a {@link FeedSurfaceCoordinator} associated with a Tab in an Activity.
 */
public class NtpFeedSurfaceLifecycleManager extends FeedSurfaceLifecycleManager {
    /** Key for the Feed instance state that may be stored in a navigation entry. */
    private static final String FEED_SAVED_INSTANCE_STATE_KEY = "FeedSavedInstanceState";

    private static PrefService sPrefServiceForTesting;

    /** The {@link Tab} that {@link #mCoordinator} is attached to. */
    private final Tab mTab;

    /**
     * The {@link TabObserver} that observes tab state changes and notifies the {@link
     * FeedSurfaceCoordinator} accordingly.
     */
    private final TabObserver mTabObserver;

    /**
     * @param activity The {@link Activity} that the {@link FeedSurfaceCoordinator} is attached to.
     * @param tab The {@link Tab} that the {@link FeedSurfaceCoordinator} is attached to.
     * @param coordinator The coordinator of the feed.
     */
    public NtpFeedSurfaceLifecycleManager(
            Activity activity, Tab tab, FeedSurfaceCoordinator coordinator) {
        super(activity, coordinator);

        // Set mTab before 'start' since 'restoreInstanceState' will use it.
        mTab = tab;
        start();

        // We don't need to handle EmptyTabObserver#onDestroy here since this class will be
        // destroyed when the associated NewTabPage is destroyed.
        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
                        if (isInteractable) {
                            show();
                        }
                    }

                    @Override
                    public void onShown(Tab tab, @TabSelectionType int type) {
                        show();
                    }

                    @Override
                    public void onHidden(Tab tab, @TabHidingType int type) {
                        hide();
                    }

                    @Override
                    public void onPageLoadStarted(Tab tab, GURL url) {
                        saveInstanceState();
                    }
                };
        mTab.addObserver(mTabObserver);
    }

    /** @return Whether the {@link FeedSurfaceCoordinator} can be shown. */
    @Override
    protected boolean canShow() {
        // We don't call FeedSurfaceCoordinator#onShow to prevent feed services from being warmed up
        // if the user has opted out from article suggestions during the previous session.
        return super.canShow()
                && getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE)
                && !mTab.isHidden();
    }

    /**
     * Clears any dependencies when this class is not needed
     * anymore.
     */
    @Override
    protected void destroy() {
        if (mSurfaceState == SurfaceState.DESTROYED) return;

        super.destroy();
        mTab.removeObserver(mTabObserver);
    }

    /** Save the Feed instance state to the navigation entry if necessary. */
    @Override
    protected void saveInstanceState() {
        if (mTab.getWebContents() == null) return;

        NavigationController controller = mTab.getWebContents().getNavigationController();
        int index = controller.getLastCommittedEntryIndex();
        NavigationEntry entry = controller.getEntryAtIndex(index);
        if (entry == null) return;

        // At least under test conditions this method may be called initially for the load of the
        // NTP itself, at which point the last committed entry is not for the NTP yet. This method
        // will then be called a second time when the user navigates away, at which point the last
        // committed entry is for the NTP. The extra data must only be set in the latter case.
        if (!UrlUtilities.isNtpUrl(entry.getUrl())) return;

        controller.setEntryExtraData(
                index, FEED_SAVED_INSTANCE_STATE_KEY, mCoordinator.getSavedInstanceStateString());
    }

    /**
     * @return The feed instance state saved in navigation entry, or null if it is not previously
     *         saved.
     */
    @Override
    protected @Nullable String restoreInstanceState() {
        if (mTab.getWebContents() == null) return null;

        NavigationController controller = mTab.getWebContents().getNavigationController();
        int index = controller.getLastCommittedEntryIndex();
        return controller.getEntryExtraData(index, FEED_SAVED_INSTANCE_STATE_KEY);
    }

    TabObserver getTabObserverForTesting() {
        return mTabObserver;
    }

    private PrefService getPrefService() {
        if (sPrefServiceForTesting != null) return sPrefServiceForTesting;
        return UserPrefs.get(mTab.getProfile());
    }

    static void setPrefServiceForTesting(PrefService prefServiceForTesting) {
        sPrefServiceForTesting = prefServiceForTesting;
        ResettersForTesting.register(() -> sPrefServiceForTesting = null);
    }
}
