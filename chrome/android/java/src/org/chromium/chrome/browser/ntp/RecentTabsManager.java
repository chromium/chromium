// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.invalidation.SessionsInvalidationManager;
import org.chromium.chrome.browser.ntp.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.ntp.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.signin.ui.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.ui.SigninPromoController;
import org.chromium.chrome.browser.signin.ui.SigninPromoUtil;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.List;

/**
 * Provides the domain logic and data for RecentTabsPage and RecentTabsRowAdapter.
 */
public class RecentTabsManager implements ProfileSyncService.SyncStateChangedListener,
                                          SignInStateObserver, ProfileDataCache.Observer,
                                          AccountsChangeObserver {
    /**
     * Implement this to receive updates when the page contents change.
     */
    interface UpdatedCallback {
        /**
         * Called when the list of recently closed tabs or foreign sessions changes.
         */
        void onUpdated();
    }
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({PromoState.PROMO_NONE, PromoState.PROMO_SIGNIN_PERSONALIZED,
            PromoState.PROMO_SYNC_PERSONALIZED, PromoState.PROMO_SYNC})
    @interface PromoState {
        int PROMO_NONE = 0;
        int PROMO_SIGNIN_PERSONALIZED = 1;
        int PROMO_SYNC_PERSONALIZED = 2;
        int PROMO_SYNC = 3;
    }

    private static final int RECENTLY_CLOSED_MAX_TAB_COUNT = 5;

    private static RecentlyClosedTabManager sRecentlyClosedTabManagerForTests;

    private final Profile mProfile;
    private final Tab mTab;
    private final Runnable mShowHistoryManager;

    private FaviconHelper mFaviconHelper;
    private ForeignSessionHelper mForeignSessionHelper;
    private List<ForeignSession> mForeignSessions;
    private List<RecentlyClosedTab> mRecentlyClosedTabs;
    private RecentTabsPagePrefs mPrefs;
    private RecentlyClosedTabManager mRecentlyClosedTabManager;
    private SigninManager mSignInManager;
    private UpdatedCallback mUpdatedCallback;
    private boolean mIsDestroyed;

    private final ProfileDataCache mProfileDataCache;
    private final SigninPromoController mSigninPromoController;
    @Nullable
    private final ProfileSyncService mProfileSyncService;

    /**
     * Create an RecentTabsManager to be used with RecentTabsPage and RecentTabsRowAdapter.
     *
     * @param tab The Tab that is showing this recent tabs page.
     * @param profile Profile that is associated with the current session.
     * @param context the Android context this manager will work in.
     * @param showHistoryManager Runnable showing history manager UI.
     */
    public RecentTabsManager(
            Tab tab, Profile profile, Context context, Runnable showHistoryManager) {
        mProfile = profile;
        mTab = tab;
        mShowHistoryManager = showHistoryManager;
        mForeignSessionHelper = new ForeignSessionHelper(profile);
        mPrefs = new RecentTabsPagePrefs(profile);
        mFaviconHelper = new FaviconHelper();
        mRecentlyClosedTabManager = sRecentlyClosedTabManagerForTests != null
                ? sRecentlyClosedTabManagerForTests
                : new RecentlyClosedBridge(profile);
        mSignInManager = IdentityServicesProvider.get().getSigninManager(mProfile);

        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(context);
        mSigninPromoController = new SigninPromoController(
                SigninAccessPoint.RECENT_TABS, SigninActivityLauncherImpl.get());
        mProfileSyncService = ProfileSyncService.get();

        mRecentlyClosedTabManager.setTabsUpdatedRunnable(() -> {
            updateRecentlyClosedTabs();
            postUpdate();
        });

        updateRecentlyClosedTabs();
        registerForForeignSessionUpdates();
        updateForeignSessions();
        mForeignSessionHelper.triggerSessionSync();
        registerObservers();

        SessionsInvalidationManager.get(mProfile).onRecentTabsPageOpened();
    }

    /**
     * Should be called when this object is no longer needed. Performs necessary listener tear down.
     */
    public void destroy() {
        mIsDestroyed = true;
        if (mProfileSyncService != null) {
            mProfileSyncService.removeSyncStateChangedListener(this);
        }

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

    private void registerForForeignSessionUpdates() {
        mForeignSessionHelper.setOnForeignSessionCallback(() -> {
            updateForeignSessions();
            postUpdate();
        });
    }

    private void registerObservers() {
        if (mProfileSyncService != null) {
            mProfileSyncService.addSyncStateChangedListener(this);
        }

        mSignInManager.addSignInStateObserver(this);

        mProfileDataCache.addObserver(this);
        AccountManagerFacadeProvider.getInstance().addObserver(this);
    }

    private void updateRecentlyClosedTabs() {
        mRecentlyClosedTabs =
                mRecentlyClosedTabManager.getRecentlyClosedTabs(RECENTLY_CLOSED_MAX_TAB_COUNT);
    }

    private void updateForeignSessions() {
        mForeignSessions = mForeignSessionHelper.getForeignSessions();
        if (mForeignSessions == null) {
            mForeignSessions = Collections.emptyList();
        }
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
    public List<RecentlyClosedTab> getRecentlyClosedTabs() {
        return mRecentlyClosedTabs;
    }

    /**
     * Opens a new tab navigating to ForeignSessionTab.
     *
     * @param session The foreign session that the tab belongs to.
     * @param tab The tab to open.
     * @param windowDisposition The WindowOpenDisposition flag.
     */
    public void openForeignSessionTab(ForeignSession session, ForeignSessionTab tab,
            int windowDisposition) {
        if (mIsDestroyed) return;
        RecordUserAction.record("MobileRecentTabManagerTabFromOtherDeviceOpened");
        mForeignSessionHelper.openForeignSessionTab(mTab, session, tab, windowDisposition);
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
        RecordUserAction.record("MobileRecentTabManagerRecentTabOpened");
        mRecentlyClosedTabManager.openRecentlyClosedTab(mTab, tab, windowDisposition);
    }

    /**
     * Opens the history page.
     */
    public void openHistoryPage() {
        if (mIsDestroyed) return;
        mShowHistoryManager.run();
    }

    /**
     * Return the managed tab.
     * @return the tab instance being managed by this object.
     */
    public Tab activeTab() {
        return mTab;
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
            String url, int size, FaviconImageCallback faviconCallback) {
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
    public boolean getLocalFaviconForUrl(String url, int size,
            FaviconImageCallback faviconCallback) {
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

    /**
     * Clears the list of recently closed tabs.
     */
    public void clearRecentlyClosedTabs() {
        if (mIsDestroyed) return;
        mRecentlyClosedTabManager.clearRecentlyClosedTabs();
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

    /**
     * @return The promo type that will be displayed on the screen.
     */
    @PromoState
    int getPromoType() {
        if (!mSignInManager.getIdentityManager().hasPrimaryAccount()) {
            if (!mSignInManager.isSignInAllowed()) {
                return PromoState.PROMO_NONE;
            }
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                    && mSignInManager.getIdentityManager().getPrimaryAccountInfo(
                               ConsentLevel.SIGNIN)
                            != null) {
                return PromoState.PROMO_SYNC_PERSONALIZED;
            }
            return PromoState.PROMO_SIGNIN_PERSONALIZED;
        }

        if (mProfileSyncService == null) {
            // |mProfileSyncService| will remain null until the next browser startup, so no sense in
            // offering any promo.
            return PromoState.PROMO_NONE;
        }

        if (mProfileSyncService.isSyncRequested() && !mForeignSessions.isEmpty()) {
            return PromoState.PROMO_NONE;
        }
        return PromoState.PROMO_SYNC;
    }

    /**
     * Sets up the personalized signin promo and records user actions for promo impressions.
     * @param view The view to be configured.
     */
    void setupPersonalizedSigninPromo(PersonalizedSigninPromoView view) {
        SigninPromoUtil.setupSigninPromoViewFromCache(
                mSigninPromoController, mProfileDataCache, view, null);
    }

    void setupPersonalizedSyncPromo(PersonalizedSigninPromoView view) {
        SigninPromoUtil.setupSyncPromoViewFromCache(
                mSigninPromoController, mProfileDataCache, view, null);
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
    public void onAccountsChanged() {
        update();
    }

    // ProfileDataCache.Observer implementation.
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        update();
    }

    // ProfileSyncService.SyncStateChangedListener implementation.
    @Override
    public void syncStateChanged() {
        update();
    }

    private void postUpdate() {
        if (mUpdatedCallback != null) {
            mUpdatedCallback.onUpdated();
        }
    }

    private void update() {
        // TODO(crbug.com/1129853): Re-evaluate whether it's necessary to post
        // a task.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            if (mIsDestroyed) return;
            updateForeignSessions();
            postUpdate();
        });
    }

    @VisibleForTesting
    public static void setRecentlyClosedTabManagerForTests(RecentlyClosedTabManager manager) {
        sRecentlyClosedTabManagerForTests = manager;
    }
}
