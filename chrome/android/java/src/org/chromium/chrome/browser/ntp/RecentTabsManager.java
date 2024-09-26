// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.invalidation.SessionsInvalidationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.SyncPromoController;
import org.chromium.chrome.browser.ui.signin.SyncPromoController.SyncPromoState;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Provides the domain logic and data for RecentTabsPage and RecentTabsRowAdapter. */
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

    private static final int RECENTLY_CLOSED_MAX_ENTRY_COUNT = 5;

    private static RecentlyClosedTabManager sRecentlyClosedTabManagerForTests;

    private final Profile mProfile;
    private final Tab mActiveTab;
    private final TabModelSelector mTabModelSelector;
    private final Runnable mShowHistoryManager;

    private TabModel mTabModel;
    private @SyncPromoState int mPromoState = SyncPromoState.NO_PROMO;
    private FaviconHelper mFaviconHelper;
    private ForeignSessionHelper mForeignSessionHelper;
    private List<ForeignSession> mForeignSessions;
    private List<RecentlyClosedEntry> mRecentlyClosedEntries;
    private RecentTabsPagePrefs mPrefs;
    private RecentlyClosedTabManager mRecentlyClosedTabManager;
    private SigninManager mSignInManager;
    private UpdatedCallback mUpdatedCallback;
    private boolean mIsDestroyed;

    private final ProfileDataCache mProfileDataCache;
    private final SyncPromoController mSyncPromoController;
    private final SyncService mSyncService;

    /**
     * Maps Session IDs to whether that entry was restored split by entry type. These are used to
     * record historgrams on {@link #destroy()} to measure restore ratio. Cached Session IDs are
     * used to de-duplicate as update would otherwise result in incorrect metrics.
     */
    private final Map<Integer, Boolean> mTabSessionIdsRestored = new HashMap<>();

    private final Map<Integer, Boolean> mGroupSessionIdsRestored = new HashMap<>();
    private final Map<Integer, Boolean> mBulkSessionIdsRestored = new HashMap<>();

    /**
     * Create an RecentTabsManager to be used with RecentTabsPage and RecentTabsRowAdapter.
     *
     * @param tab The Tab that is showing this recent tabs page.
     * @param tabModelSelector The TabModelSelector that contains or will contain {@code tab}.
     * @param profile Profile that is associated with the current session.
     * @param context the Android context this manager will work in.
     * @param showHistoryManager Runnable showing history manager UI.
     */
    public RecentTabsManager(
            Tab tab,
            TabModelSelector tabModelSelector,
            Profile profile,
            Context context,
            Runnable showHistoryManager) {
        mProfile = profile;
        mActiveTab = tab;
        mTabModelSelector = tabModelSelector;
        mShowHistoryManager = showHistoryManager;
        mForeignSessionHelper = new ForeignSessionHelper(profile);
        mPrefs = new RecentTabsPagePrefs(profile);
        mFaviconHelper = new FaviconHelper();
        mRecentlyClosedTabManager =
                sRecentlyClosedTabManagerForTests != null
                        ? sRecentlyClosedTabManagerForTests
                        : new RecentlyClosedBridge(profile, tabModelSelector);
        mSignInManager = IdentityServicesProvider.get().getSigninManager(mProfile);

        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(context);
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(
                                R.string.signin_account_picker_bottom_sheet_title)
                        .build();
        mSyncPromoController =
                new SyncPromoController(
                        mProfile,
                        bottomSheetStrings,
                        SigninAccessPoint.RECENT_TABS,
                        SyncConsentActivityLauncherImpl.get(),
                        SigninAndHistorySyncActivityLauncherImpl.get());
        mSyncService = SyncServiceFactory.getForProfile(mProfile);

        mRecentlyClosedTabManager.setEntriesUpdatedRunnable(this::updateRecentlyClosedEntries);
        updateRecentlyClosedEntries();

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
            final int percentRestored = Math.round((restoredCount * 100.0f) / shownCount);
            RecordHistogram.recordPercentageHistogram(
                    "Tabs.RecentlyClosed.PercentOfEntriesRestoredInPage." + entryType,
                    percentRestored);
        }
    }

    /**
     * Should be called when this object is no longer needed. Performs necessary listener tear down.
     */
    public void destroy() {
        mIsDestroyed = true;

        recordEntries("Tab", mTabSessionIdsRestored);
        recordEntries("Group", mGroupSessionIdsRestored);
        recordEntries("Bulk", mBulkSessionIdsRestored);

        mSyncService.removeSyncStateChangedListener(this);

        mSignInManager.removeSignInStateObserver(this);
        mSignInManager = null;

        mProfileDataCache.removeObserver(this);
        AccountManagerFacadeProvider.getInstance().removeObserver(this);

        mFaviconHelper.destroy();
        mFaviconHelper = null;

        mRecentlyClosedTabManager.destroy();
        mRecentlyClosedTabManager = null;

        mUpdatedCallback = null;

        mPrefs.destroy();
        mPrefs = null;

        SessionsInvalidationManager.get(mProfile).onRecentTabsPageClosed();

        mForeignSessionHelper.destroy();
        mForeignSessionHelper = null;
    }

    private void updateRecentlyClosedEntries() {
        mRecentlyClosedEntries =
                mRecentlyClosedTabManager.getRecentlyClosedEntries(RECENTLY_CLOSED_MAX_ENTRY_COUNT);
        for (RecentlyClosedEntry entry : mRecentlyClosedEntries) {
            if (entry instanceof RecentlyClosedTab
                    && !mTabSessionIdsRestored.containsKey(entry.getSessionId())) {
                mTabSessionIdsRestored.put(entry.getSessionId(), false);
            } else if (entry instanceof RecentlyClosedGroup
                    && !mGroupSessionIdsRestored.containsKey(entry.getSessionId())) {
                mGroupSessionIdsRestored.put(entry.getSessionId(), false);
            } else if (entry instanceof RecentlyClosedBulkEvent
                    && !mBulkSessionIdsRestored.containsKey(entry.getSessionId())) {
                mBulkSessionIdsRestored.put(entry.getSessionId(), false);
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
        return mRecentlyClosedEntries;
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
        mRecentlyClosedTabManager.openRecentlyClosedTab(getTabModel(), tab, windowDisposition);
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

        if (entry instanceof RecentlyClosedGroup) {
            mGroupSessionIdsRestored.put(entry.getSessionId(), true);
            RecordUserAction.record("MobileRecentTabManagerRecentGroupOpened");
        } else if (entry instanceof RecentlyClosedBulkEvent) {
            mBulkSessionIdsRestored.put(entry.getSessionId(), true);
            RecordUserAction.record("MobileRecentTabManagerRecentBulkEventOpened");
        }
        mRecentlyClosedTabManager.openRecentlyClosedEntry(getTabModel(), entry);
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
        mRecentlyClosedTabManager.clearRecentlyClosedEntries();
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

    /** Returns the current promo state. */
    @SyncPromoState
    int getPromoState() {
        return mPromoState;
    }

    private @SyncPromoState int calculatePromoState() {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            // If ReplaceSyncPromosWithSignInPromos is enabled, there's only one promo type.
            //
            // TODO(crbug.com/343908771): Revise SyncPromoState after launching
            //     ReplaceSyncPromosWithSignInPromos.
            if (!mSyncPromoController.canShowSyncPromo()) {
                return SyncPromoState.NO_PROMO;
            }
            return SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE;
        }
        if (!mSignInManager.getIdentityManager().hasPrimaryAccount(ConsentLevel.SYNC)) {
            if (!mSyncPromoController.canShowSyncPromo()) {
                return SyncPromoState.NO_PROMO;
            }
            // TODO(crbug.com/338541375): Move this check inside
            //  SyncPromoController#canShowSyncPromo().
            if (!mSignInManager.isSyncOptInAllowed()) {
                return SyncPromoState.NO_PROMO;
            }
            if (mSignInManager.getIdentityManager().hasPrimaryAccount(ConsentLevel.SIGNIN)) {
                return SyncPromoState.PROMO_FOR_SIGNED_IN_STATE;
            }
            return SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE;
        }

        if (!mForeignSessions.isEmpty()) {
            return SyncPromoState.NO_PROMO;
        }

        // TODO(crbug.com/40850972): PROMO_FOR_SYNC_TURNED_OFF_STATE should only
        // be returned if mSyncService.getSelectedTypes().isEmpty(). Otherwise,
        // LegacySyncPromoView incorrectly displays a promo with string
        // R.string.ntp_recent_tabs_sync_promo_instructions.
        return SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE;
    }

    private void updatePromoState() {
        final @SyncPromoState int newState = calculatePromoState();
        if (newState == mPromoState) return;

        final boolean hasSyncPromoStateChangedtoShown =
                (mPromoState == SyncPromoState.NO_PROMO
                                || mPromoState == SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE)
                        && (newState == SyncPromoState.PROMO_FOR_SIGNED_IN_STATE
                                || newState == SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE);
        if (hasSyncPromoStateChangedtoShown) {
            mSyncPromoController.increasePromoShowCount();
        }
        mPromoState = newState;
    }

    /** Sets up the sync promo view. */
    void setUpSyncPromoView(PersonalizedSigninPromoView view) {
        mSyncPromoController.setUpSyncPromoView(mProfileDataCache, view, null);
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
    public void onProfileDataUpdated(String accountEmail) {
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
        updatePromoState();
        if (mIsDestroyed) return;
        updateForeignSessions();
        onUpdateDone();
    }

    private TabModel getTabModel() {
        // When RecentTabsManager is created for a new tab then {@link mActiveTab} is being
        // created and will not be present in a {@link TabModel} of {@link mTabModelSelector}.
        // Defer finding the {@link TabModel} until the first time it is needed after the
        // constructor has finished.
        if (mTabModel != null) return mTabModel;

        mTabModel = mTabModelSelector.getModelForTabId(mActiveTab.getId());
        assert mTabModel != null;
        return mTabModel;
    }

    public static void setRecentlyClosedTabManagerForTests(RecentlyClosedTabManager manager) {
        sRecentlyClosedTabManagerForTests = manager;
        ResettersForTesting.register(() -> sRecentlyClosedTabManagerForTests = null);
    }
}
