// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.TerminationStatus;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.SwipeRefreshHandler;
import org.chromium.chrome.browser.app.bluetooth.BluetoothNotificationService;
import org.chromium.chrome.browser.app.usb.UsbNotificationService;
import org.chromium.chrome.browser.bluetooth.BluetoothNotificationManager;
import org.chromium.chrome.browser.display_cutout.DisplayCutoutTabHelper;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.media.MediaCaptureNotificationServiceImpl;
import org.chromium.chrome.browser.pdf.PdfUtils;
import org.chromium.chrome.browser.policy.PolicyAuditor;
import org.chromium.chrome.browser.policy.PolicyAuditor.AuditEvent;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.usb.UsbNotificationManager;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.net.NetError;
import org.chromium.ui.mojom.VirtualKeyboardMode;
import org.chromium.url.GURL;

/** WebContentsObserver used by Tab. */
public class TabWebContentsObserver extends TabWebContentsUserData {
    // URL didFailLoad error code. Should match the value in net_error_list.h.
    public static final int BLOCKED_BY_ADMINISTRATOR = -22;

    private static final Class<TabWebContentsObserver> USER_DATA_KEY = TabWebContentsObserver.class;

    /** Used for logging. */
    private static final String TAG = "TabWebContentsObs";

    private final TabImpl mTab;
    private final ObserverList<Callback<WebContents>> mInitObservers = new ObserverList<>();
    private WebContentsObserver mObserver;
    private GURL mLastUrl;

    public static TabWebContentsObserver from(Tab tab) {
        TabWebContentsObserver observer = get(tab);
        if (observer == null) {
            observer = new TabWebContentsObserver(tab);
            tab.getUserDataHost().setUserData(USER_DATA_KEY, observer);
        }
        return observer;
    }

    @VisibleForTesting
    public static TabWebContentsObserver get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private TabWebContentsObserver(Tab tab) {
        super(tab);
        mTab = (TabImpl) tab;
    }

    /**
     * Adds an observer triggered when |initWebContents| is invoked.
     * <p>A newly created tab adding this observer misses the event because
     * |TabObserver.onContentChanged| -&gt; |TabWebContentsObserver.initWebContents|
     * occurs before the observer is added. Manually trigger it here.
     * @param observer Observer to add.
     */
    public void addInitWebContentsObserver(Callback<WebContents> observer) {
        if (mInitObservers.addObserver(observer) && mTab.getWebContents() != null) {
            observer.onResult(mTab.getWebContents());
        }
    }

    /** Remove the InitWebContents observer from the list. */
    public void removeInitWebContentsObserver(Callback<WebContents> observer) {
        mInitObservers.removeObserver(observer);
    }

    @Override
    public void destroyInternal() {
        mInitObservers.clear();
    }

    @Override
    public void initWebContents(WebContents webContents) {
        mObserver = new Observer(webContents);

        for (Callback<WebContents> callback : mInitObservers) callback.onResult(webContents);
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {
        if (mObserver != null) {
            mObserver.destroy();
            mObserver = null;
        }
    }

    public void simulateRendererKilledForTesting() {
        if (mObserver != null) {
            mObserver.primaryMainFrameRenderProcessGone(TerminationStatus.PROCESS_WAS_KILLED);
        }
    }

    private void showSadTab(SadTab sadTab) {
        sadTab.show(
                mTab.getThemedApplicationContext(),
                /* suggestionAction= */ () -> {
                    Activity activity = mTab.getWindowAndroid().getActivity().get();
                    assert activity != null;
                    HelpAndFeedbackLauncherImpl.getForProfile(mTab.getProfile())
                            .show(
                                    activity,
                                    activity.getString(R.string.help_context_sad_tab),
                                    null);
                },

                /* buttonAction= */ () -> {
                    if (sadTab.showSendFeedbackView()) {
                        mTab.getActivity()
                                .startHelpAndFeedback(
                                        mTab.getUrl().getSpec(),
                                        "MobileSadTabFeedback",
                                        mTab.getProfile());
                    } else {
                        mTab.reload();
                    }
                });
    }

    private class Observer extends WebContentsObserver {
        public Observer(WebContents webContents) {
            super(webContents);
        }

        @Override
        public void primaryMainFrameRenderProcessGone(@TerminationStatus int terminationStatus) {
            Log.i(
                    TAG,
                    "primaryMainFrameRenderProcessGone() for tab id: "
                            + mTab.getId()
                            + ", already needs reload: "
                            + Boolean.toString(mTab.needsReload())
                            + ", termination status: "
                            + Integer.toString(terminationStatus));
            // Do nothing for subsequent calls that happen while the tab remains crashed. This
            // can occur when the tab is in the background and it shares the renderer with other
            // tabs. After the renderer crashes, the WebContents of its tabs are still around
            // and they still share the RenderProcessHost. When one of the tabs reloads spawning
            // a new renderer for the shared RenderProcessHost and the new renderer crashes
            // again, all tabs sharing this renderer will be notified about the crash (including
            // potential background tabs that did not reload yet).
            if (mTab.needsReload() || SadTab.isShowing(mTab)) return;

            // If the renderer crashes for a native page, it can be ignored as we never show that
            // content. The URL check is done in addition to the isNativePage to ensure a navigation
            // off the native page did not result in the crash.
            if (mTab.isNativePage()
                    && (mTab.getNativePage().getUrl().equals(mTab.getUrl().getSpec())
                            || NativePage.isNativePageUrl(
                                    mTab.getUrl(),
                                    mTab.isIncognito(),
                                    PdfUtils.shouldOpenPdfInline(mTab.isIncognito())
                                            && PdfUtils.isDownloadedPdf(
                                                    mTab.getUrl().getSpec())))) {
                return;
            }

            int activityState =
                    ApplicationStatus.getStateForActivity(
                            mTab.getWindowAndroid().getActivity().get());
            if (mTab.isHidden()
                    || activityState == ActivityState.PAUSED
                    || activityState == ActivityState.STOPPED
                    || activityState == ActivityState.DESTROYED) {
                // The tab crashed in background or was killed by the OS out-of-memory killer.
                mTab.setNeedsReload();
            } else {
                // TODO(crbug.com/40127852): Remove the PostTask and call SadTab directly when
                // WebContentsObserverProxy observers' iterator concurrency issue is fixed.
                // Showing the SadTab will cause the content view hosting WebContents to lose focus.
                // Post the show in order to avoid immediately triggering
                // {@link WebContentsObserver#onWebContentsLostFocus}. This will ensure all
                // observers in {@link WebContentsObserverProxy} receive callbacks for
                // {@link WebContentsObserver#primaryMainFrameRenderProcessGone} first.
                SadTab sadTab = SadTab.from(mTab);
                PostTask.postTask(TaskTraits.UI_DEFAULT, () -> showSadTab(sadTab));
                // This is necessary to correlate histogram data with stability counts.
                RecordHistogram.recordBooleanHistogram("Stability.Android.RendererCrash", true);
            }

            mTab.handleTabCrash();
        }

        @Override
        public void didFinishLoadInPrimaryMainFrame(
                GlobalRenderFrameHostId frameId,
                GURL url,
                boolean isKnownValid,
                @LifecycleState int frameLifecycleState) {
            assert isKnownValid;
            if (frameLifecycleState == LifecycleState.ACTIVE) {
                if (mTab.getNativePage() != null) {
                    mTab.pushNativePageStateToNavigationEntry();
                }
                mTab.didFinishPageLoad(url);
            }
        }

        @Override
        public void didFailLoad(
                boolean isInPrimaryMainFrame,
                int errorCode,
                GURL failingGurl,
                @LifecycleState int frameLifecycleState) {
            if (isInPrimaryMainFrame) {
                mTab.didFailPageLoad(errorCode);
            }

            recordErrorInPolicyAuditor(failingGurl.getSpec(), "net error: " + errorCode, errorCode);
        }

        private void recordErrorInPolicyAuditor(
                String failingUrl, String description, int errorCode) {
            assert description != null;

            PolicyAuditor auditor = PolicyAuditor.maybeCreate();
            if (auditor != null) {
                auditor.notifyAuditEvent(
                        ContextUtils.getApplicationContext(),
                        AuditEvent.OPEN_URL_FAILURE,
                        failingUrl,
                        description);
                if (errorCode == BLOCKED_BY_ADMINISTRATOR) {
                    auditor.notifyAuditEvent(
                            ContextUtils.getApplicationContext(),
                            AuditEvent.OPEN_URL_BLOCKED,
                            failingUrl,
                            "");
                }
            }
        }

        @Override
        public void titleWasSet(String title) {
            mTab.updateTitle(title);
        }

        @Override
        public void didStartNavigationInPrimaryMainFrame(NavigationHandle navigation) {
            if (!navigation.isSameDocument()) {
                mTab.didStartPageLoad(navigation.getUrl());
            }

            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) {
                observers.next().onDidStartNavigationInPrimaryMainFrame(mTab, navigation);
            }
        }

        @Override
        public void didRedirectNavigation(NavigationHandle navigation) {
            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) {
                observers.next().onDidRedirectNavigation(mTab, navigation);
            }
        }

        @Override
        public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) {
                observers.next().onDidFinishNavigationInPrimaryMainFrame(mTab, navigation);
            }

            if (navigation.errorCode() != NetError.OK) {
                mTab.didFailPageLoad(navigation.errorCode());
            }
            mLastUrl = navigation.getUrl();

            if (!navigation.hasCommitted()) return;

            mTab.updateTitle();
            mTab.handleDidFinishNavigation(
                    navigation.getUrl(), navigation.pageTransition(), navigation.isPdf());
            mTab.setIsShowingErrorPage(navigation.isErrorPage());

            // TODO(crbug.com/40264745) remove this call. onUrlUpdated should have been called
            // by NotifyNavigationStateChanged, which is always called before didFinishNavigation
            observers.rewind();
            while (observers.hasNext()) {
                observers.next().onUrlUpdated(mTab);
            }

            // Stop swipe-to-refresh animation.
            SwipeRefreshHandler handler = SwipeRefreshHandler.get(mTab);
            if (handler != null) handler.didStopRefreshing();

            // TODO(crbug.com/40264745) add this here to clear LocationBarModel's cache for
            // being in a same site navigation. Remove this call when the onUrlUpdated call
            // above is removed.
            observers.rewind();
            while (observers.hasNext()) {
                observers.next().onDidFinishNavigationEnd();
            }
        }

        @Override
        public void loadProgressChanged(float progress) {
            if (!mTab.isLoading()) return;
            mTab.notifyLoadProgress(progress);
        }

        @Override
        public void didFirstVisuallyNonEmptyPaint() {
            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            mTab.notifyDidFirstVisuallyNonEmptyPaint();
            while (observers.hasNext()) {
                observers.next().didFirstVisuallyNonEmptyPaint(mTab);
            }
        }

        @Override
        public void didChangeThemeColor() {
            mTab.updateThemeColor(mTab.getWebContents().getThemeColor());
        }

        @Override
        public void onBackgroundColorChanged() {
            if (ChromeFeatureList.sNavBarColorMatchesTabBackground.isEnabled()) {
                mTab.changeWebContentBackgroundColor(mTab.getWebContents().getBackgroundColor());
            }
        }

        @Override
        public void navigationEntriesDeleted() {
            mTab.notifyNavigationEntriesDeleted();
        }

        @Override
        public void viewportFitChanged(@WebContentsObserver.ViewportFitType int value) {
            DisplayCutoutTabHelper.from(mTab).setViewportFit(value);
        }

        @Override
        public void virtualKeyboardModeChanged(@VirtualKeyboardMode.EnumType int mode) {
            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) {
                observers.next().onVirtualKeyboardModeChanged(mTab, mode);
            }
        }

        @Override
        public void destroy() {
            MediaCaptureNotificationServiceImpl.updateMediaNotificationForTab(
                    ContextUtils.getApplicationContext(), mTab.getId(), null, mLastUrl);
            BluetoothNotificationManager.updateBluetoothNotificationForTab(
                    ContextUtils.getApplicationContext(),
                    BluetoothNotificationService.class,
                    mTab.getId(),
                    null,
                    mLastUrl,
                    mTab.isIncognito());
            UsbNotificationManager.updateUsbNotificationForTab(
                    ContextUtils.getApplicationContext(),
                    UsbNotificationService.class,
                    mTab.getId(),
                    null,
                    mLastUrl,
                    mTab.isIncognito());
            super.destroy();
        }
    }
}
