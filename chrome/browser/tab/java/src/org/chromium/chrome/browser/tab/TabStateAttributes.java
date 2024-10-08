// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.Token;
import org.chromium.base.UserDataHost;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Predicate;

/** Attributes related to {@link TabState} */
public class TabStateAttributes extends TabWebContentsUserData {
    private static final Class<TabStateAttributes> USER_DATA_KEY = TabStateAttributes.class;
    @VisibleForTesting static final long DEFAULT_LOW_PRIORITY_SAVE_DELAY_MS = 30 * 1000L;

    /**
     * Defines the dirtiness states of the tab attributes. Numerical values should be setup such
     * that higher values are more dirty.
     */
    @IntDef({DirtinessState.CLEAN, DirtinessState.UNTIDY, DirtinessState.DIRTY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DirtinessState {
        /** The state of the tab has no meaningful changes. */
        int CLEAN = 0;

        /** The state of the tab has slight changes. */
        int UNTIDY = 1;

        /** The state of the tab has significant changes. */
        int DIRTY = 2;
    }

    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final Tab mTab;

    /** Whether or not the TabState has changed. */
    private @DirtinessState int mDirtinessState = DirtinessState.CLEAN;

    private WebContentsObserver mWebContentsObserver;
    private boolean mPendingLowPrioritySave;

    /**
     * When this number is greater than zero, all dirty observations are currently being suppressed.
     * Using an int instead of boolean to support reentrancy.
     */
    private int mNumberOpenBatchEdits;

    /** The most dirty end state transition that's been had during the current changes, or null. */
    private @Nullable Integer mPendingDirty;

    /** Allows observing changes for Tab state dirtiness updates. */
    public interface Observer {
        /**
         * Triggered when the tab state dirtiness has changed.
         *
         * @param tab The tab whose state has changed.
         * @param dirtiness Tne state of dirtiness for the tab state.
         */
        void onTabStateDirtinessChanged(Tab tab, @DirtinessState int dirtiness);
    }

    /**
     * Creates the {@link TabStateAttributes} for the given {@link Tab}.
     * @param tab The Tab reference whose state this is associated with.
     * @param creationState The creation state of the tab (if it exists).
     */
    public static void createForTab(Tab tab, @Nullable @TabCreationState Integer creationState) {
        UserDataHost host = tab.getUserDataHost();
        host.setUserData(USER_DATA_KEY, new TabStateAttributes(tab, creationState));
    }

    /**
     * @return {@link TabStateAttributes} for a {@link Tab}
     */
    public static TabStateAttributes from(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        return host.getUserData(USER_DATA_KEY);
    }

    private TabStateAttributes(Tab tab, @Nullable @TabCreationState Integer creationState) {
        super(tab);
        mTab = tab;
        if (creationState == null || creationState == TabCreationState.FROZEN_FOR_LAZY_LOAD) {
            updateIsDirty(DirtinessState.DIRTY);
        } else if (creationState == TabCreationState.LIVE_IN_FOREGROUND
                || creationState == TabCreationState.LIVE_IN_BACKGROUND) {
            updateIsDirty(DirtinessState.UNTIDY);
        } else {
            assert creationState == TabCreationState.FROZEN_ON_RESTORE;
        }
        // TODO(crbug.com/40242471): Should this also handle mTab.getPendingLoadParams(), and ignore
        //                      URL updates when the URL matches the pending load?
        mTab.addObserver(
                new EmptyTabObserver() {
                    @Override
                    public void onHidden(Tab tab, int reason) {
                        if (!mTab.isClosing() && mDirtinessState == DirtinessState.UNTIDY) {
                            updateIsDirty(DirtinessState.DIRTY);
                        }
                    }

                    @Override
                    public void onClosingStateChanged(Tab tab, boolean closing) {
                        if (!closing && mDirtinessState == DirtinessState.UNTIDY) {
                            updateIsDirty(DirtinessState.DIRTY);
                        }
                    }

                    @Override
                    public void onNavigationEntriesAppended(Tab tab) {
                        updateIsDirty(DirtinessState.DIRTY);
                    }

                    @Override
                    public void onNavigationEntriesDeleted(Tab tab) {
                        updateIsDirty(DirtinessState.DIRTY);
                    }

                    @Override
                    public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                        if (mDirtinessState != DirtinessState.UNTIDY) return;

                        if (toDifferentDocument) {
                            updateIsDirty(DirtinessState.DIRTY);
                        } else {
                            if (mPendingLowPrioritySave) return;
                            mPendingLowPrioritySave = true;
                            PostTask.postDelayedTask(
                                    TaskTraits.UI_DEFAULT,
                                    () -> {
                                        assert mPendingLowPrioritySave;
                                        if (mDirtinessState == DirtinessState.UNTIDY) {
                                            updateIsDirty(DirtinessState.DIRTY);
                                        }
                                        mPendingLowPrioritySave = false;
                                    },
                                    DEFAULT_LOW_PRIORITY_SAVE_DELAY_MS);
                        }
                    }

                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        // TODO(crbug.com/40242471): Reconcile the overlapping calls of
                        //                      didFinishNavigationInPrimaryMainFrame,
                        // onPageLoadFinished,
                        //                      and onLoadStopped.
                        updateIsDirty(DirtinessState.UNTIDY);
                    }

                    @Override
                    public void onTitleUpdated(Tab tab) {
                        // TODO(crbug.com/40242471): Is the title of a page normally received before
                        //                      onLoadStopped? If not, this will get marked as
                        // untidy
                        //                      soon after the initial page load.
                        updateIsDirty(DirtinessState.UNTIDY);
                    }

                    @Override
                    public void onActivityAttachmentChanged(Tab tab, WindowAndroid window) {
                        if (window == null) return;
                        updateIsDirty(DirtinessState.UNTIDY);
                    }

                    @Override
                    public void onRootIdChanged(Tab tab, int newRootId) {
                        if (!tab.isInitialized()) return;
                        updateIsDirtyNotCheckingNtp(DirtinessState.DIRTY);
                    }

                    @Override
                    public void onTabGroupIdChanged(Tab tab, @Nullable Token tabGroupId) {
                        if (!tab.isInitialized()) return;
                        updateIsDirtyNotCheckingNtp(DirtinessState.DIRTY);
                    }

                    @Override
                    public void onTabContentSensitivityChanged(
                            Tab tab, boolean contentIsSensitive) {
                        if (!tab.isInitialized()) return;
                        updateIsDirty(DirtinessState.UNTIDY);
                    }
                });
    }

    @Override
    public void initWebContents(WebContents webContents) {
        mWebContentsObserver =
                new WebContentsObserver(webContents) {
                    @Override
                    public void navigationEntriesChanged() {
                        updateIsDirty(DirtinessState.UNTIDY);
                    }

                    @Override
                    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                        updateIsDirty(DirtinessState.UNTIDY);
                    }
                };
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
        }
    }

    /**
     * @return true if the {@link TabState} has been changed
     */
    public @DirtinessState int getDirtinessState() {
        return mDirtinessState;
    }

    /** Signals that the tab state is no longer dirty (e.g. has been successfully persisted). */
    public void clearTabStateDirtiness() {
        updateIsDirty(DirtinessState.CLEAN);
    }

    @VisibleForTesting
    void updateIsDirty(@DirtinessState int dirtiness) {
        updateIsDirtyInternal(
                dirtiness, tab -> isTabUrlContentScheme(tab) || isNtpWithoutNavigationState(tab));
    }

    /**
     * Same as {@link updateIsDirty} but does not check whether the tab is an NTP with navigation
     * state.
     */
    @VisibleForTesting
    void updateIsDirtyNotCheckingNtp(@DirtinessState int dirtiness) {
        updateIsDirtyInternal(dirtiness, TabStateAttributes::isTabUrlContentScheme);
    }

    /**
     * Tries to update {@link DirtinessState} if applicable and notifies observers of any changes.
     *
     * @param dirtiness The new {@link DirtinessState} to set.
     * @param shouldSetToClean A predicate determining whether to set the dirtiness to clean.
     */
    private void updateIsDirtyInternal(
            @DirtinessState int dirtiness, @NonNull Predicate<Tab> shouldSetToClean) {
        if (mTab.isDestroyed()) return;
        if (dirtiness == mDirtinessState) return;
        if (mTab.isBeingRestored()) return;

        // Do not lower dirtiness to UNTIDY from DIRTY as we should wait for the state to be CLEAN.
        if (dirtiness == DirtinessState.UNTIDY && mDirtinessState == DirtinessState.DIRTY) {
            return;
        }

        if (shouldSetToClean.test(mTab)) {
            if (mDirtinessState == DirtinessState.CLEAN) return;

            mDirtinessState = DirtinessState.CLEAN;
        } else {
            mDirtinessState = dirtiness;
        }

        if (mNumberOpenBatchEdits > 0) {
            updatePendingDirty(mDirtinessState);
        } else {
            for (Observer observer : mObservers) {
                observer.onTabStateDirtinessChanged(mTab, mDirtinessState);
            }
        }
    }

    private static boolean isTabUrlContentScheme(@NonNull Tab tab) {
        GURL url = tab.getUrl();
        return url != null && url.getScheme().equals(UrlConstants.CONTENT_SCHEME);
    }

    private static boolean isNtpWithoutNavigationState(@NonNull Tab tab) {
        return UrlUtilities.isNtpUrl(tab.getUrl()) && !tab.canGoBack() && !tab.canGoForward();
    }

    /**
     * Registers an observer for tab state dirtiness updates.
     * @param obs The observer to be added.
     * @return The current dirtiness state.
     */
    public @DirtinessState int addObserver(Observer obs) {
        mObservers.addObserver(obs);
        return mDirtinessState;
    }

    /**
     * Removes a tab state dirtiness observer.
     * @param obs The observer to be removed.
     */
    public void removeObserver(Observer obs) {
        mObservers.removeObserver(obs);
    }

    /**
     * Temporarily suppress dirty observations while multiple changes are made to this tab. This can
     * be helpful to coalesce multiple changes into a single write to persistent storage. If you
     * call this method you must call {@link #endBatchEdit()} as soon as changes are done being
     * made, otherwise this tab will be left in a broken state.
     */
    public void beginBatchEdit() {
        mNumberOpenBatchEdits++;
    }

    public void endBatchEdit() {
        mNumberOpenBatchEdits--;
        assert mNumberOpenBatchEdits >= 0;
        if (mNumberOpenBatchEdits == 0 && mPendingDirty != null) {
            // Reset mPendingDirty just in case we need to support reentrancy from observers.
            int pendingDirtyCopy = mPendingDirty;
            mPendingDirty = null;
            for (Observer observer : mObservers) {
                observer.onTabStateDirtinessChanged(mTab, pendingDirtyCopy);
            }
        }
    }

    private void updatePendingDirty(@DirtinessState int newDirty) {
        mPendingDirty =
                mPendingDirty == null ? newDirty : Math.max(mPendingDirty.intValue(), newDirty);
    }

    /** Allows overriding the current value for tests. */
    public void setStateForTesting(@DirtinessState int dirtiness) {
        var oldValue = mDirtinessState;
        mDirtinessState = dirtiness;
        ResettersForTesting.register(() -> mDirtinessState = oldValue);
    }
}
