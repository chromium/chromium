// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.shared.stream.Stream;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
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
 * Manages the lifecycle of a {@link Stream} associated with a Tab in an Activity.
 */
public class NtpStreamLifecycleManager extends StreamLifecycleManager {
    /** Key for the Stream instance state that may be stored in a navigation entry. */
    private static final String STREAM_SAVED_INSTANCE_STATE_KEY = "StreamSavedInstanceState";

    private static PrefService sPrefServiceForTesting;

    /** The {@link Tab} that {@link #mStream} is attached to. */
    private final Tab mTab;

    /**
     * The {@link TabObserver} that observes tab state changes and notifies the {@link Stream}
     * accordingly.
     */
    private final TabObserver mTabObserver;

    /**
     * @param stream The {@link Stream} that this class manages.
     * @param activity The {@link Activity} that the {@link Stream} is attached to.
     * @param tab The {@link Tab} that the {@link Stream} is attached to.
     */
    public NtpStreamLifecycleManager(Stream stream, Activity activity, Tab tab) {
        super(stream, activity);

        // Set mTab before 'start' since 'restoreInstanceState' will use it.
        mTab = tab;
        start();

        // We don't need to handle mStream#onDestroy here since this class will be destroyed when
        // the associated NewTabPage is destroyed.
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
                if (isInteractable) {
                    activate();
                } else {
                    deactivate();
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

    /** @return Whether the {@link Stream} can be shown. */
    @Override
    protected boolean canShow() {
        // We don't call Stream#onShow to prevent feed services from being warmed up if the user
        // has opted out from article suggestions during the previous session.
        return super.canShow() && getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE)
                && !mTab.isHidden();
    }

    /** @return Whether the {@link Stream} can be activated. */
    @Override
    protected boolean canActivate() {
        return super.canActivate() && getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE)
                && mTab.isUserInteractable();
    }

    /**
     * Clears any dependencies and calls {@link Stream#onDestroy()} when this class is not needed
     * anymore.
     */
    @Override
    protected void destroy() {
        if (mStreamState == StreamState.DESTROYED) return;

        super.destroy();
        mTab.removeObserver(mTabObserver);
    }

    /** Save the Stream instance state to the navigation entry if necessary. */
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
        if (!UrlUtilities.isNTPUrl(entry.getUrl())) return;

        controller.setEntryExtraData(
                index, STREAM_SAVED_INSTANCE_STATE_KEY, mStream.getSavedInstanceStateString());
    }

    /**
     * @return The Stream instance state saved in navigation entry, or null if it is not previously
     *         saved.
     */
    @Override
    @Nullable
    protected String restoreInstanceState() {
        if (mTab.getWebContents() == null) return null;

        NavigationController controller = mTab.getWebContents().getNavigationController();
        int index = controller.getLastCommittedEntryIndex();
        return controller.getEntryExtraData(index, STREAM_SAVED_INSTANCE_STATE_KEY);
    }

    @VisibleForTesting
    TabObserver getTabObserverForTesting() {
        return mTabObserver;
    }

    private PrefService getPrefService() {
        if (sPrefServiceForTesting != null) return sPrefServiceForTesting;
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    @VisibleForTesting
    static void setPrefServiceForTesting(PrefService prefServiceForTesting) {
        sPrefServiceForTesting = prefServiceForTesting;
    }
}
