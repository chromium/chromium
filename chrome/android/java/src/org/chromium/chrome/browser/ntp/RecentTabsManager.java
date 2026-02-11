// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.RecentlyClosedEntriesManager;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.invalidation.SessionsInvalidationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.signin_promo.RecentTabsSigninPromoDelegate;
import org.chromium.chrome.browser.ui.signin.signin_promo.SigninPromoCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Supplier;

/** Provides the domain logic and data for RecentTabsPage and RecentTabsRowAdapter. */
@NullMarked
public class RecentTabsManager
        implements SyncService.SyncStateChangedListener,
                SignInStateObserver,
                ProfileDataCache.Observer,
                AccountsChangeObserver {
    /** Implement this to receive updates when the page contents change. */
    interface UpdatedCallback {
        /** Called when the list of recently closed tabs or foreign sessions changes. */
        void onUpdated();
    }

    private final Profile mProfile;
    private final Tab mActiveTab;
    private final Runnable mShowHistoryManager;
    private final SigninPromoCoordinator mSigninPromoCoordinator;
    private FaviconHelper mFaviconHelper;
    private ForeignSessionHelper mForeignSessionHelper;
    private List<ForeignSession> mForeignSessions;
    private RecentTabsPagePrefs mPrefs;
    private SigninManager mSignInManager;
    private @Nullable UpdatedCallback mUpdatedCallback;
    private @Nullable View mSigninPromoView;
    private boolean mShouldShowPromo;
    private boolean mIsDestroyed;

    private final ProfileDataCache mProfileDataCache;
    private final SyncService mSyncService;
    private final RecentlyClosedEntriesManager mRecentlyClosedEntriesManager;

    /**
     * Maps Session IDs to whether that entry was restored split by entry type. These are used to
     * record historgrams on {@link #destroy()} to measure restore ratio. Cached Session IDs are
     * used to de-duplicate as update would otherwise result in incorrect metrics.
     */
    private final Map<Integer, Boolean> mTabSessionIdsRestored = new HashMap<>();

    private final Map<Integer, Boolean> mGroupSessionIdsRestored = new HashMap<>();
    private final Map<Integer, Boolean> mBulkSessionIdsRestored = new HashMap<>();
    private final Map<Integer, Boolean> mWindowInstanceIdsRestored = new HashMap<>();

    /**
     * Create an RecentTabsManager to be used with RecentTabsPage and RecentTabsRowAdapter.
     *
     * @param tab The Tab that is showing this recent tabs page.
     * @param windowAndroid The window showing this recent tabs page.
     * @param activity The Android Activity this manager will work in.
     * @param profile Profile that is associated with the current session.
     * @param activityResultTracker Tracker of activity results.
     * @param bottomSheetController Used to interact with the bottom sheet.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     * @param snackbarManager Manages snackbars shown in the app.
     * @param showHistoryManager Runnable showing history manager UI.
     */
    public RecentTabsManager(
            Tab tab,
            WindowAndroid windowAndroid,
            Activity activity,
            Profile profile,
            ActivityResultTracker activityResultTracker,
            BottomSheetController bottomSheetController,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            SnackbarManager snackbarManager,
            Runnable showHistoryManager,
            RecentlyClosedEntriesManager recentlyClosedEntriesManager) {
        mProfile = profile;
        mActiveTab = tab;
        mShowHistoryManager = showHistoryManager;
        mForeignSessionHelper = new ForeignSessionHelper(profile);
        mPrefs = new RecentTabsPagePrefs(profile);
        mFaviconHelper = new FaviconHelper();
        mRecentlyClosedEntriesManager = recentlyClosedEntriesManager;
        mSignInManager = assumeNonNull(IdentityServicesProvider.get().getSigninManager(mProfile));

        mProfileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                        activity, mSignInManager.getIdentityManager());
        mSigninPromoCoordinator =
                new SigninPromoCoordinator(
                        windowAndroid,
                        activity,
                        profile,
                        activityResultTracker,
                        SigninAndHistorySyncActivityLauncherImpl.get(),
                        SupplierUtils.of(bottomSheetController),
                        modalDialogManagerSupplier,
                        snackbarManager,
                        DeviceLockActivityLauncherImpl.get(),
                        new RecentTabsSigninPromoDelegate(
                                activity,
                                profile,
                                SigninAndHistorySyncActivityLauncherImpl.get(),
                                this::updatePromoState));
        mSyncService = assumeNonNull(SyncServiceFactory.getForProfile(mProfile));

        mRecentlyClosedEntriesManager.setEntriesUpdatedCallback(
                (recentlyClosedEntries) -> updateRecentlyClosedEntries(recentlyClosedEntries));
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        mForeignSessionHelper.setOnForeignSessionCallback(this::updateForeignSessions);
        updateForeignSessions();
        mForeignSessionHelper.triggerSessionSync();

        mSyncService.addSyncStateChangedListener(this);
        mSignInManager.addSignInStateObserver(this);
        mProfileDataCache.addObserver(this);
        AccountManagerFacadeProvider.getInstance().addObserver(this);
        updatePromoState();

        SessionsInvalidationManager.get(mProfile).onRecentTabsPageOpened();
    }

    /** Return the {@link Profile} associated with the recent tabs. */
    public Profile getProfile() {
        return mProfile;
    }

    private static int countSessionIdsRestored(Map<Integer, Boolean> sessionIdToRestoredState) {
        int count = 0;
        for (Boolean state : sessionIdToRestoredState.values()) {
            count += state ? 1 : 0;
        }
        return count;
    }

    private static void recordEntries(
            String entryType, Map<Integer, Boolean> sessionIdToRestoredState) {
        final int shownCount = sessionIdToRestoredState.size();
        RecordHistogram.recordCount1000Histogram(
                "Tabs.RecentlyClosed.EntriesShownInPage." + entryType, shownCount);
        if (shownCount > 0) {
            final int restoredCount = countSessionIdsRestored(sessionIdToRestoredState);
            RecordHistogram.recordCount1000Histogram(
                    "Tabs.RecentlyClosed.EntriesRestoredInPage." + entryType, restoredCount);
        }
    }

    /**
     * Should be called when this object is no longer needed. Performs necessary listener tear down.
     */
    @SuppressWarnings("NullAway")
    public void destroy() {
        mIsDestroyed = true;

        recordEntries("Tab", mTabSessionIdsRestored);
        recordEntries("Group", mGroupSessionIdsRestored);
        recordEntries("Bulk", mBulkSessionIdsRestored);
        recordEntries("Window", mWindowInstanceIdsRestored);

        mSyncService.removeSyncStateChangedListener(this);
        if (mSigninPromoCoordinator != null) {
            mSigninPromoCoordinator.destroy();
        }

        mSignInManager.removeSignInStateObserver(this);
        mSignInManager = null;

        mProfileDataCache.removeObserver(this);
        AccountManagerFacadeProvider.getInstance().removeObserver(this);

        mFaviconHelper.destroy();
        mFaviconHelper = null;

        mUpdatedCallback = null;

        mPrefs.destroy();
        mPrefs = null;

        SessionsInvalidationManager.get(mProfile).onRecentTabsPageClosed();

        mForeignSessionHelper.destroy();
        mForeignSessionHelper = null;
    }

    private void updateRecentlyClosedEntries(List<RecentlyClosedEntry> entries) {
        for (RecentlyClosedEntry entry : entries) {
            if (entry instanceof RecentlyClosedTab closedTab
                    && !mTabSessionIdsRestored.containsKey(closedTab.getSessionId())) {
                mTabSessionIdsRestored.put(closedTab.getSessionId(), false);
            } else if (entry instanceof RecentlyClosedGroup closedGroup
                    && !mGroupSessionIdsRestored.containsKey(closedGroup.getSessionId())) {
                mGroupSessionIdsRestored.put(closedGroup.getSessionId(), false);
            } else if (entry instanceof RecentlyClosedBulkEvent closedBulkEvent
                    && !mBulkSessionIdsRestored.containsKey(closedBulkEvent.getSessionId())) {
                mBulkSessionIdsRestored.put(closedBulkEvent.getSessionId(), false);
            } else if (entry instanceof RecentlyClosedWindow closedWindow
                    && !mWindowInstanceIdsRestored.containsKey(closedWindow.getInstanceId())) {
                mWindowInstanceIdsRestored.put(closedWindow.getInstanceId(), false);
            }
        }
        onUpdateDone();
    }

    private void updateForeignSessions() {
        mForeignSessions = mForeignSessionHelper.getForeignSessions();
        onUpdateDone();
    }

    /**
     * @return Most up-to-date list of foreign sessions.
     */
    public List<ForeignSession> getForeignSessions() {
        return mForeignSessions;
    }

    /**
     * @return Most up-to-date list of recently closed tabs.
     */
    public List<RecentlyClosedEntry> getRecentlyClosedEntries() {
        return mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
    }

    /**
     * Opens a new tab navigating to ForeignSessionTab.
     *
     * @param session The foreign session that the tab belongs to.
     * @param tab The tab to open.
     * @param windowDisposition The WindowOpenDisposition flag.
     */
    public void openForeignSessionTab(
            ForeignSession session, ForeignSessionTab tab, int windowDisposition) {
        if (mIsDestroyed) return;
        RecordUserAction.record("MobileRecentTabManagerTabFromOtherDeviceOpened");
        RecordUserAction.record("MobileCrossDeviceTabJourney");
        mForeignSessionHelper.openForeignSessionTab(mActiveTab, session, tab, windowDisposition);
    }

    /**
     * Restores a recently closed tab.
     *
     * @param tab The tab to open.
     * @param windowDisposition The WindowOpenDisposition value specifying whether the tab should
     *         be restored into the current tab or a new tab.
     */
    public void openRecentlyClosedTab(RecentlyClosedTab tab, int windowDisposition) {
        if (mIsDestroyed) return;
        mTabSessionIdsRestored.put(tab.getSessionId(), true);
        RecordUserAction.record("MobileRecentTabManagerRecentTabOpened");
        // Window disposition will select which tab to open.
        mRecentlyClosedEntriesManager.openRecentlyClosedTab(tab, windowDisposition);
    }

    /**
     * Restores a recently closed entry. Use {@link #openRecentlyClosedTab()} for single tabs..
     *
     * @param entry The entry to open.
     */
    public void openRecentlyClosedEntry(RecentlyClosedEntry entry) {
        if (mIsDestroyed) return;

        assert !(entry instanceof RecentlyClosedTab)
                : "Opening a RecentlyClosedTab should use openRecentlyClosedTab().";

        if (entry instanceof RecentlyClosedGroup closedGroup) {
            mGroupSessionIdsRestored.put(closedGroup.getSessionId(), true);
            RecordUserAction.record("MobileRecentTabManagerRecentGroupOpened");
        } else if (entry instanceof RecentlyClosedBulkEvent closedBulkEvent) {
            mBulkSessionIdsRestored.put(closedBulkEvent.getSessionId(), true);
            RecordUserAction.record("MobileRecentTabManagerRecentBulkEventOpened");
        } else if (entry instanceof RecentlyClosedWindow closedWindow) {
            int instanceId = closedWindow.getInstanceId();
            mWindowInstanceIdsRestored.put(instanceId, true);
            RecordUserAction.record("MobileRecentTabManagerRecentWindowOpened");
        }
        mRecentlyClosedEntriesManager.openRecentlyClosedEntry(entry);
    }

    /** Opens the history page. */
    public void openHistoryPage() {
        if (mIsDestroyed) return;
        mShowHistoryManager.run();
    }

    /**
     * Return the managed tab.
     * @return the tab instance being managed by this object.
     */
    public Tab activeTab() {
        return mActiveTab;
    }

    /**
     * Returns a favicon for a given foreign url.
     *
     * @param url The url to fetch the favicon for.
     * @param size the desired favicon size.
     * @param faviconCallback the callback to be invoked when the favicon is available.
     * @return favicon or null if favicon unavailable.
     */
    public boolean getForeignFaviconForUrl(
            GURL url, int size, FaviconImageCallback faviconCallback) {
        return mFaviconHelper.getForeignFaviconImageForURL(mProfile, url, size, faviconCallback);
    }

    /**
     * Fetches a favicon for snapshot document url which is returned via callback.
     *
     * @param url The url to fetch a favicon for.
     * @param size the desired favicon size.
     * @param faviconCallback the callback to be invoked when the favicon is available.
     *
     * @return may return false if we could not fetch the favicon.
     */
    public boolean getLocalFaviconForUrl(GURL url, int size, FaviconImageCallback faviconCallback) {
        return mFaviconHelper.getLocalFaviconImageForURL(mProfile, url, size, faviconCallback);
    }

    /**
     * Sets a callback to be invoked when recently closed tabs or foreign sessions documents have
     * been updated.
     *
     * @param updatedCallback the listener to be invoked.
     */
    public void setUpdatedCallback(UpdatedCallback updatedCallback) {
        mUpdatedCallback = updatedCallback;
    }

    /**
     * Sets the persistent expanded/collapsed state of a foreign session list.
     *
     * @param session foreign session to collapsed.
     * @param isCollapsed Whether the session is collapsed or expanded.
     */
    public void setForeignSessionCollapsed(ForeignSession session, boolean isCollapsed) {
        if (mIsDestroyed) return;
        mPrefs.setForeignSessionCollapsed(session, isCollapsed);
    }

    /**
     * Determine the expanded/collapsed state of a foreign session list.
     *
     * @param session foreign session whose state to obtain.
     *
     * @return Whether the session is collapsed.
     */
    public boolean getForeignSessionCollapsed(ForeignSession session) {
        return mPrefs.getForeignSessionCollapsed(session);
    }

    /**
     * Sets the persistent expanded/collapsed state of the recently closed tabs list.
     *
     * @param isCollapsed Whether the recently closed tabs list is collapsed.
     */
    public void setRecentlyClosedTabsCollapsed(boolean isCollapsed) {
        if (mIsDestroyed) return;
        mPrefs.setRecentlyClosedTabsCollapsed(isCollapsed);
    }

    /**
     * Determine the expanded/collapsed state of the recently closed tabs list.
     *
     * @return Whether the recently closed tabs list is collapsed.
     */
    public boolean isRecentlyClosedTabsCollapsed() {
        return mPrefs.getRecentlyClosedTabsCollapsed();
    }

    /**
     * Remove Foreign session to display. Note that it might reappear during the next sync if the
     * session is not orphaned.
     *
     * This is mainly for when user wants to delete an orphaned session.
     * @param session Session to be deleted.
     */
    public void deleteForeignSession(ForeignSession session) {
        if (mIsDestroyed) return;
        mForeignSessionHelper.deleteForeignSession(session);
    }

    /** Clears the list of recently closed tabs. */
    public void clearRecentlyClosedEntries() {
        if (mIsDestroyed) return;
        RecordUserAction.record("MobileRecentTabManagerRecentTabsCleared");
        mRecentlyClosedEntriesManager.clearRecentlyClosedEntries();
    }

    /**
     * Collapse the promo.
     *
     * @param isCollapsed Whether the promo is collapsed.
     */
    public void setPromoCollapsed(boolean isCollapsed) {
        if (mIsDestroyed) return;
        mPrefs.setSyncPromoCollapsed(isCollapsed);
    }

    /**
     * Determine whether the promo is collapsed.
     *
     * @return Whether the promo is collapsed.
     */
    public boolean isPromoCollapsed() {
        return mPrefs.getSyncPromoCollapsed();
    }

    /** Returns whether the promo should be shown or not. */
    boolean shouldShowPromo() {
        return mShouldShowPromo;
    }

    View getSigninPromoView(ViewGroup parent) {
        if (mSigninPromoView == null) {
            mSigninPromoView = mSigninPromoCoordinator.buildPromoView(parent);
            mSigninPromoCoordinator.setView(mSigninPromoView);
        }
        return mSigninPromoView;
    }

    // SignInStateObserver implementation.
    @Override
    public void onSignedIn() {
        update();
    }

    @Override
    public void onSignedOut() {
        update();
    }

    // AccountsChangeObserver implementation.
    @Override
    public void onCoreAccountInfosChanged() {
        update();
    }

    // ProfileDataCache.Observer implementation.
    @Override
    public void onProfileDataUpdated(DisplayableProfileData profileData) {
        update();
    }

    // SyncService.SyncStateChangedListener implementation.
    @Override
    public void syncStateChanged() {
        update();
    }

    private void onUpdateDone() {
        if (mUpdatedCallback != null) {
            mUpdatedCallback.onUpdated();
        }
    }

    private void update() {
        if (mIsDestroyed) return;
        updateForeignSessions();
        onUpdateDone();
    }

    private void updatePromoState() {
        final boolean shouldShowPromo = mSigninPromoCoordinator.canShowPromo();
        if (shouldShowPromo == mShouldShowPromo) return;
        mShouldShowPromo = shouldShowPromo;

        if (mIsDestroyed) return;
        onUpdateDone();
    }
}
