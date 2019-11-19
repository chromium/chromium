// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.annotation.SuppressLint;
import android.os.Handler;
import android.os.Message;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.modaldialog.TabModalPresenter;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;

/**
 * Determines the desired visibility of the browser controls based on the current state of a given
 * tab.
 */
public class TabStateBrowserControlsVisibilityDelegate
        implements BrowserControlsVisibilityDelegate {
    protected static final int MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD = 1;
    /** The maximum amount of time to wait for a page to load before entering fullscreen. */
    private static final long MAX_FULLSCREEN_LOAD_DELAY_MS = 3000;

    private static boolean sDisableLoadingCheck;

    protected final Tab mTab;

    private boolean mIsFullscreenWaitingForLoad;

    /**
     * Basic constructor.
     * @param tab The associated {@link Tab}.
     */
    public TabStateBrowserControlsVisibilityDelegate(Tab tab) {
        mTab = tab;

        mTab.addObserver(new EmptyTabObserver() {
            @SuppressLint("HandlerLeak")
            private Handler mHandler = new Handler() {
                @Override
                public void handleMessage(Message msg) {
                    if (msg == null) return;
                    if (msg.what == MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD) {
                        enableFullscreenAfterLoad();
                    }
                }
            };

            private long getLoadDelayMs() {
                return sDisableLoadingCheck ? 0 : MAX_FULLSCREEN_LOAD_DELAY_MS;
            }

            private void enableFullscreenAfterLoad() {
                if (!mIsFullscreenWaitingForLoad) return;

                mIsFullscreenWaitingForLoad = false;
                TabBrowserControlsState.updateEnabledState(mTab);
            }

            private void cancelEnableFullscreenLoadDelay() {
                mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);
                mIsFullscreenWaitingForLoad = false;
            }

            private void scheduleEnableFullscreenLoadDelayIfNecessary() {
                if (mIsFullscreenWaitingForLoad
                        && !mHandler.hasMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD)) {
                    mHandler.sendEmptyMessageDelayed(
                            MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD, getLoadDelayMs());
                }
            }

            @Override
            public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                if (!didStartLoad) return;

                // As we may have missed the main frame commit notification for the
                // swapped web contents, schedule the enabling of fullscreen now.
                scheduleEnableFullscreenLoadDelayIfNecessary();
            }

            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                if (!navigation.hasCommitted() || !navigation.isInMainFrame()) return;
                mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);
                mHandler.sendEmptyMessageDelayed(
                        MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD, getLoadDelayMs());
                TabBrowserControlsState.updateEnabledState(mTab);
            }

            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                mIsFullscreenWaitingForLoad = !DomDistillerUrlUtils.isDistilledPage(url);
                TabBrowserControlsState.updateEnabledState(mTab);
            }

            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                // Handle the case where a commit or prerender swap notification failed to arrive
                // and the enable fullscreen message was never enqueued.
                scheduleEnableFullscreenLoadDelayIfNecessary();
                TabBrowserControlsState.updateEnabledState(mTab);
            }

            @Override
            public void onPageLoadFailed(Tab tab, int errorCode) {
                cancelEnableFullscreenLoadDelay();
                TabBrowserControlsState.updateEnabledState(mTab);
            }

            @Override
            public void onHidden(Tab tab, @TabHidingType int type) {
                cancelEnableFullscreenLoadDelay();
            }

            @Override
            public void onDestroyed(Tab tab) {
                super.onDestroyed(tab);

                // Remove pending handler actions to prevent memory leaks.
                mHandler.removeCallbacksAndMessages(null);
            }
        });
    }

    @Override
    public boolean canShowBrowserControls() {
        return true;
    }

    @Override
    public boolean canAutoHideBrowserControls() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DONT_AUTO_HIDE_BROWSER_CONTROLS)
                && mTab.getActivity() != null && mTab.getActivity().getToolbarManager() != null
                && mTab.getActivity().getToolbarManager().getBottomToolbarCoordinator() != null) {
            return false;
        }

        WebContents webContents = mTab.getWebContents();
        if (webContents == null || webContents.isDestroyed()) return false;

        String url = mTab.getUrl();
        boolean enableHidingBrowserControls = url != null;
        enableHidingBrowserControls &= !url.startsWith(UrlConstants.CHROME_URL_PREFIX);
        enableHidingBrowserControls &= !url.startsWith(UrlConstants.CHROME_NATIVE_URL_PREFIX);

        int securityState = mTab.getSecurityLevel();
        enableHidingBrowserControls &= (securityState != ConnectionSecurityLevel.DANGEROUS);

        enableHidingBrowserControls &= !AccessibilityUtil.isAccessibilityEnabled();

        enableHidingBrowserControls &=
                !SelectionPopupController.fromWebContents(webContents).isFocusedNodeEditable();
        enableHidingBrowserControls &= !mTab.isShowingErrorPage();
        enableHidingBrowserControls &= !webContents.isShowingInterstitialPage();
        enableHidingBrowserControls &= !mTab.isRendererUnresponsive();
        enableHidingBrowserControls &= !mTab.isHidden();
        enableHidingBrowserControls &= DeviceClassManager.enableFullscreen();
        enableHidingBrowserControls &= !mIsFullscreenWaitingForLoad;
        enableHidingBrowserControls &= !TabModalPresenter.isDialogShowing(mTab);

        return enableHidingBrowserControls;
    }

    /**
     * Disables the logic that prevents hiding the top controls during page load for testing.
     */
    public static void disablePageLoadDelayForTests() {
        sDisableLoadingCheck = true;
    }
}
