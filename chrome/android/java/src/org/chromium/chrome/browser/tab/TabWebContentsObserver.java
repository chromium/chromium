// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.support.annotation.IntDef;
import android.view.View;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.SwipeRefreshHandler;
import org.chromium.chrome.browser.display_cutout.DisplayCutoutController;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.media.MediaCaptureNotificationService;
import org.chromium.chrome.browser.policy.PolicyAuditor;
import org.chromium.chrome.browser.policy.PolicyAuditor.AuditEvent;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * WebContentsObserver used by Tab.
 */
public class TabWebContentsObserver extends TabWebContentsUserData {
    // URL didFailLoad error code. Should match the value in net_error_list.h.
    public static final int BLOCKED_BY_ADMINISTRATOR = -22;

    private static final Class<TabWebContentsObserver> USER_DATA_KEY = TabWebContentsObserver.class;

    /** Used for logging. */
    private static final String TAG = "TabWebContentsObs";

    // TabRendererCrashStatus defined in tools/metrics/histograms/histograms.xml.
    private static final int TAB_RENDERER_CRASH_STATUS_SHOWN_IN_FOREGROUND_APP = 0;
    private static final int TAB_RENDERER_CRASH_STATUS_HIDDEN_IN_FOREGROUND_APP = 1;
    private static final int TAB_RENDERER_CRASH_STATUS_HIDDEN_IN_BACKGROUND_APP = 2;
    private static final int TAB_RENDERER_CRASH_STATUS_MAX = 3;

    // TabRendererExitStatus defined in tools/metrics/histograms/histograms.xml.
    // Designed to replace TabRendererCrashStatus if numbers line up.
    @IntDef({TabRendererExitStatus.OOM_PROTECTED_IN_RUNNING_APP,
            TabRendererExitStatus.OOM_PROTECTED_IN_PAUSED_APP,
            TabRendererExitStatus.OOM_PROTECTED_IN_BACKGROUND_APP,
            TabRendererExitStatus.NOT_PROTECTED_IN_RUNNING_APP,
            TabRendererExitStatus.NOT_PROTECTED_IN_PAUSED_APP,
            TabRendererExitStatus.NOT_PROTECTED_IN_BACKGROUND_APP})
    @Retention(RetentionPolicy.SOURCE)
    private @interface TabRendererExitStatus {
        int OOM_PROTECTED_IN_RUNNING_APP = 0;
        int OOM_PROTECTED_IN_PAUSED_APP = 1;
        int OOM_PROTECTED_IN_BACKGROUND_APP = 2;
        int NOT_PROTECTED_IN_RUNNING_APP = 3;
        int NOT_PROTECTED_IN_PAUSED_APP = 4;
        int NOT_PROTECTED_IN_BACKGROUND_APP = 5;
        int NUM_ENTRIES = 6;
    }

    private final Tab mTab;
    private WebContentsObserver mObserver;

    public static void from(Tab tab) {
        TabWebContentsObserver observer = get(tab);
        if (observer == null) {
            tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabWebContentsObserver(tab));
        }
    }

    @VisibleForTesting
    public static TabWebContentsObserver get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private TabWebContentsObserver(Tab tab) {
        super(tab);
        mTab = tab;
    }

    @Override
    public void initWebContents(WebContents webContents) {
        mObserver = new Observer(webContents);
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {
        mObserver.destroy();
        mObserver = null;
    }

    @VisibleForTesting
    public void simulateRendererKilledForTesting(boolean wasOomProtected) {
        if (mObserver != null) mObserver.renderProcessGone(wasOomProtected);
    }

    private class Observer extends WebContentsObserver {
        public Observer(WebContents webContents) {
            super(webContents);
        }

        @Override
        public void renderProcessGone(boolean processWasOomProtected) {
            Log.i(TAG,
                    "renderProcessGone() for tab id: " + mTab.getId()
                            + ", oom protected: " + Boolean.toString(processWasOomProtected)
                            + ", already needs reload: " + Boolean.toString(mTab.needsReload()));
            // Do nothing for subsequent calls that happen while the tab remains crashed. This
            // can occur when the tab is in the background and it shares the renderer with other
            // tabs. After the renderer crashes, the WebContents of its tabs are still around
            // and they still share the RenderProcessHost. When one of the tabs reloads spawning
            // a new renderer for the shared RenderProcessHost and the new renderer crashes
            // again, all tabs sharing this renderer will be notified about the crash (including
            // potential background tabs that did not reload yet).
            if (mTab.needsReload() || SadTab.isShowing(mTab)) return;

            // This will replace TabRendererCrashStatus if numbers line up.
            int appState = ApplicationStatus.getStateForApplication();
            boolean applicationRunning = (appState == ApplicationState.HAS_RUNNING_ACTIVITIES);
            boolean applicationPaused = (appState == ApplicationState.HAS_PAUSED_ACTIVITIES);
            @TabRendererExitStatus
            int rendererExitStatus;
            if (processWasOomProtected) {
                if (applicationRunning) {
                    rendererExitStatus = TabRendererExitStatus.OOM_PROTECTED_IN_RUNNING_APP;
                } else if (applicationPaused) {
                    rendererExitStatus = TabRendererExitStatus.OOM_PROTECTED_IN_PAUSED_APP;
                } else {
                    rendererExitStatus = TabRendererExitStatus.OOM_PROTECTED_IN_BACKGROUND_APP;
                }
            } else {
                if (applicationRunning) {
                    rendererExitStatus = TabRendererExitStatus.NOT_PROTECTED_IN_RUNNING_APP;
                } else if (applicationPaused) {
                    rendererExitStatus = TabRendererExitStatus.NOT_PROTECTED_IN_PAUSED_APP;
                } else {
                    rendererExitStatus = TabRendererExitStatus.NOT_PROTECTED_IN_BACKGROUND_APP;
                }
            }
            RecordHistogram.recordEnumeratedHistogram("Tab.RendererExitStatus", rendererExitStatus,
                    TabRendererExitStatus.NUM_ENTRIES);

            int activityState = ApplicationStatus.getStateForActivity(
                    mTab.getWindowAndroid().getActivity().get());
            int rendererCrashStatus = TAB_RENDERER_CRASH_STATUS_MAX;
            if (mTab.isHidden() || activityState == ActivityState.PAUSED
                    || activityState == ActivityState.STOPPED
                    || activityState == ActivityState.DESTROYED) {
                // The tab crashed in background or was killed by the OS out-of-memory killer.
                mTab.setNeedsReload();
                if (applicationRunning) {
                    rendererCrashStatus = TAB_RENDERER_CRASH_STATUS_HIDDEN_IN_FOREGROUND_APP;
                } else {
                    rendererCrashStatus = TAB_RENDERER_CRASH_STATUS_HIDDEN_IN_BACKGROUND_APP;
                }
            } else {
                rendererCrashStatus = TAB_RENDERER_CRASH_STATUS_SHOWN_IN_FOREGROUND_APP;
                SadTab.from(mTab).show();
                // This is necessary to correlate histogram data with stability counts.
                RecordHistogram.recordBooleanHistogram("Stability.Android.RendererCrash", true);
            }
            RecordHistogram.recordEnumeratedHistogram(
                    "Tab.RendererCrashStatus", rendererCrashStatus, TAB_RENDERER_CRASH_STATUS_MAX);

            mTab.handleTabCrash();
        }

        @Override
        public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {
            if (mTab.getNativePage() != null) {
                mTab.pushNativePageStateToNavigationEntry();
            }
            if (isMainFrame) mTab.didFinishPageLoad();
            PolicyAuditor auditor = AppHooks.get().getPolicyAuditor();
            auditor.notifyAuditEvent(
                    mTab.getApplicationContext(), AuditEvent.OPEN_URL_SUCCESS, validatedUrl, "");
        }

        @Override
        public void didFailLoad(
                boolean isMainFrame, int errorCode, String description, String failingUrl) {
            mTab.updateThemeColorIfNeeded(true);
            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) {
                observers.next().onDidFailLoad(
                        mTab, isMainFrame, errorCode, description, failingUrl);
            }

            if (isMainFrame) mTab.didFailPageLoad(errorCode);

            recordErrorInPolicyAuditor(failingUrl, description, errorCode);
        }

        private void recordErrorInPolicyAuditor(
                String failingUrl, String description, int errorCode) {
            assert description != null;

            PolicyAuditor auditor = AppHooks.get().getPolicyAuditor();
            auditor.notifyAuditEvent(mTab.getApplicationContext(), AuditEvent.OPEN_URL_FAILURE,
                    failingUrl, description);
            if (errorCode == BLOCKED_BY_ADMINISTRATOR) {
                auditor.notifyAuditEvent(
                        mTab.getApplicationContext(), AuditEvent.OPEN_URL_BLOCKED, failingUrl, "");
            }
        }

        @Override
        public void titleWasSet(String title) {
            mTab.updateTitle(title);
        }

        @Override
        public void didStartNavigation(
                String url, boolean isInMainFrame, boolean isSameDocument, boolean isErrorPage) {
            if (isInMainFrame && !isSameDocument) {
                mTab.didStartPageLoad(url, isErrorPage);
            }

            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) {
                observers.next().onDidStartNavigation(
                        mTab, url, isInMainFrame, isSameDocument, isErrorPage);
            }
        }

        @Override
        public void didFinishNavigation(String url, boolean isInMainFrame, boolean isErrorPage,
                boolean hasCommitted, boolean isSameDocument, boolean isFragmentNavigation,
                boolean isRendererInitiated, boolean isDownload, Integer pageTransition,
                int errorCode, String errorDescription, int httpStatusCode) {
            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) {
                observers.next().onDidFinishNavigation(mTab, url, isInMainFrame, isErrorPage,
                        hasCommitted, isSameDocument, isFragmentNavigation, pageTransition,
                        errorCode, httpStatusCode);
            }

            if (errorCode != 0) {
                mTab.updateThemeColorIfNeeded(true);
                if (isInMainFrame) mTab.didFailPageLoad(errorCode);

                recordErrorInPolicyAuditor(url, errorDescription, errorCode);
            }

            if (!hasCommitted) return;

            if (isInMainFrame) {
                mTab.setIsTabStateDirty(true);
                mTab.updateTitle();
                mTab.handleDidFinishNavigation(url, pageTransition);
                mTab.setIsShowingErrorPage(isErrorPage);

                observers.rewind();
                while (observers.hasNext()) {
                    observers.next().onUrlUpdated(mTab);
                }
            }

            FullscreenManager fullscreenManager = mTab.getFullscreenManager();
            if (isInMainFrame && !isSameDocument && fullscreenManager != null) {
                fullscreenManager.exitPersistentFullscreenMode();
            }

            if (isInMainFrame) {
                // Stop swipe-to-refresh animation.
                SwipeRefreshHandler handler = SwipeRefreshHandler.get(mTab);
                if (handler != null) handler.didStopRefreshing();
            }
        }

        @Override
        public void didFirstVisuallyNonEmptyPaint() {
            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) {
                observers.next().didFirstVisuallyNonEmptyPaint(mTab);
            }
        }

        @Override
        public void didChangeThemeColor(int color) {
            mTab.updateThemeColorIfNeeded(true);
        }

        @Override
        public void didAttachInterstitialPage() {
            InfoBarContainer.get(mTab).setVisibility(View.INVISIBLE);
            mTab.showRenderedPage();
            mTab.updateThemeColorIfNeeded(false);

            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) {
                observers.next().onDidAttachInterstitialPage(mTab);
            }
            mTab.notifyLoadProgress(mTab.getProgress());

            mTab.updateFullscreenEnabledState();

            PolicyAuditor auditor = AppHooks.get().getPolicyAuditor();
            auditor.notifyCertificateFailure(
                    PolicyAuditor.nativeGetCertificateFailure(mTab.getWebContents()),
                    mTab.getApplicationContext());
        }

        @Override
        public void didDetachInterstitialPage() {
            InfoBarContainer.get(mTab).setVisibility(View.VISIBLE);
            mTab.updateThemeColorIfNeeded(false);

            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) {
                observers.next().onDidDetachInterstitialPage(mTab);
            }
            mTab.notifyLoadProgress(mTab.getProgress());

            mTab.updateFullscreenEnabledState();

            if (!mTab.maybeShowNativePage(mTab.getUrl(), false)) {
                mTab.showRenderedPage();
            }
        }

        @Override
        public void navigationEntriesDeleted() {
            mTab.notifyNavigationEntriesDeleted();
        }

        @Override
        public void viewportFitChanged(@WebContentsObserver.ViewportFitType int value) {
            DisplayCutoutController.from(mTab).setViewportFit(value);
        }

        @Override
        public void didReloadLoFiImages() {
            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) {
                observers.next().didReloadLoFiImages(mTab);
            }
        }

        @Override
        public void destroy() {
            MediaCaptureNotificationService.updateMediaNotificationForTab(
                    mTab.getApplicationContext(), mTab.getId(), 0, mTab.getUrl());
            super.destroy();
        }
    }
}
