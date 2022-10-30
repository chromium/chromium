// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Handler;
import android.view.KeyEvent;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.app.bluetooth.BluetoothNotificationService;
import org.chromium.chrome.browser.bluetooth.BluetoothNotificationManager;
import org.chromium.chrome.browser.media.MediaCaptureNotificationServiceImpl;
import com.ark.browser.core.utils.PolicyAuditor;
import com.ark.browser.core.utils.PolicyAuditorJni;
import org.chromium.components.find_in_page.FindMatchRectsDetails;
import org.chromium.components.find_in_page.FindNotificationDetails;
import org.chromium.content_public.browser.InvalidateTypes;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.url.GURL;

/**
 * Implementation class of {@link TabWebContentsDelegateAndroid}.
 */
public final class TabWebContentsDelegateAndroidImpl extends TabWebContentsDelegateAndroid {
    private final Tab mTab;
    private TabWebContentsDelegateAndroid mDelegate;
    private final Handler mHandler;
    private final Runnable mCloseContentsRunnable;

    public TabWebContentsDelegateAndroidImpl(Tab tab, TabWebContentsDelegateAndroid delegate) {
        mTab = tab;
        mDelegate = delegate;
        mHandler = new Handler();
        mCloseContentsRunnable = () -> {
            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) observers.next().onCloseContents(mTab);
        };
    }

    public void setDelegate(TabWebContentsDelegateAndroid mDelegate) {
        this.mDelegate = mDelegate;
    }

    @CalledByNative
    private void onFindResultAvailable(FindNotificationDetails result) {
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) observers.next().onFindResultAvailable(result);
    }

    @CalledByNative
    private void onFindMatchRectsAvailable(FindMatchRectsDetails result) {
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) observers.next().onFindMatchRectsAvailable(result);
    }

    // Helper functions used to create types that are part of the public interface
    @CalledByNative
    private static Rect createRect(int x, int y, int right, int bottom) {
        return new Rect(x, y, right, bottom);
    }

    @CalledByNative
    private static RectF createRectF(float x, float y, float right, float bottom) {
        return new RectF(x, y, right, bottom);
    }

    @CalledByNative
    private static FindNotificationDetails createFindNotificationDetails(int numberOfMatches,
            Rect rendererSelectionRect, int activeMatchOrdinal, boolean finalUpdate) {
        return new FindNotificationDetails(
                numberOfMatches, rendererSelectionRect, activeMatchOrdinal, finalUpdate);
    }

    @CalledByNative
    private static FindMatchRectsDetails createFindMatchRectsDetails(
            int version, int numRects, RectF activeRect) {
        return new FindMatchRectsDetails(version, numRects, activeRect);
    }

    @CalledByNative
    private static void setMatchRectByIndex(
            FindMatchRectsDetails findMatchRectsDetails, int index, RectF rect) {
        findMatchRectsDetails.rects[index] = rect;
    }

    @Override
    public int getDisplayMode() {
        if (mDelegate == null) {
            return super.getDisplayMode();
        }
        return mDelegate.getDisplayMode();
    }

    @CalledByNative
    @Override
    public boolean shouldResumeRequestsForCreatedWindow() {
        if (mDelegate == null) {
            return false;
        }
        return mDelegate.shouldResumeRequestsForCreatedWindow();
    }

    @CalledByNative
    @Override
    public boolean addNewContents(WebContents sourceWebContents, WebContents webContents,
            int disposition, Rect initialPosition, boolean userGesture) {
        if (mDelegate == null) {
            return false;
        }
        return mDelegate.addNewContents(
                sourceWebContents, webContents, disposition, initialPosition, userGesture);
    }

    // WebContentsDelegateAndroid

    @Override
    public void openNewTab(GURL url, String extraHeaders, ResourceRequestBody postData,
            int disposition, boolean isRendererInitiated) {
        if (mDelegate == null) {
            return;
        }
        mDelegate.openNewTab(url, extraHeaders, postData, disposition, isRendererInitiated);
    }

    @Override
    public void activateContents() {
        if (mDelegate == null) {
            return;
        }
        mDelegate.activateContents();
    }

    @Override
    public boolean addMessageToConsole(int level, String message, int lineNumber, String sourceId) {
        // Only output console.log messages on debug variants of Android OS. crbug/869804
        return !BuildInfo.isDebugAndroid();
    }

    @Override
    public void loadingStateChanged(boolean shouldShowLoadingUI) {
        mTab.loadingStateChanged(shouldShowLoadingUI);
        if (mDelegate != null) {
            mDelegate.loadingStateChanged(shouldShowLoadingUI);
        }
    }

    @Override
    public void onUpdateUrl(GURL url) {
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) observers.next().onUpdateUrl(mTab, url);
        if (mDelegate == null) {
            return;
        }
        mDelegate.onUpdateUrl(url);
    }

    @Override
    public boolean takeFocus(boolean reverse) {
        if (mDelegate == null) {
            return false;
        }
        return mDelegate.takeFocus(reverse);
    }

    @Override
    public void handleKeyboardEvent(KeyEvent event) {
        if (mDelegate == null) {
            return;
        }
        mDelegate.handleKeyboardEvent(event);
    }

    @Override
    public void enterFullscreenModeForTab(boolean prefersNavigationBar, boolean prefersStatusBar) {
        if (mDelegate == null) {
            return;
        }
        mDelegate.enterFullscreenModeForTab(prefersNavigationBar, prefersStatusBar);
    }

    @Override
    public void fullscreenStateChangedForTab(
            boolean prefersNavigationBar, boolean prefersStatusBar) {
        if (mDelegate == null) {
            return;
        }
        mDelegate.enterFullscreenModeForTab(prefersNavigationBar, prefersStatusBar);
    }

    @Override
    public void exitFullscreenModeForTab() {
        if (mDelegate == null) {
            return;
        }
        mDelegate.exitFullscreenModeForTab();
    }

    @Override
    public boolean isFullscreenForTabOrPending() {
        if (mDelegate == null) {
            return false;
        }
        return mDelegate.isFullscreenForTabOrPending();
    }

    @Override
    public void navigationStateChanged(int flags) {
        if ((flags & InvalidateTypes.TAB) != 0) {
            MediaCaptureNotificationServiceImpl.updateMediaNotificationForTab(
                    ContextUtils.getApplicationContext(), mTab.getId(), mTab.getWebContents(),
                    mTab.getUrl());
            BluetoothNotificationManager.updateBluetoothNotificationForTab(
                    ContextUtils.getApplicationContext(), BluetoothNotificationService.class,
                    mTab.getId(), mTab.getWebContents(), mTab.getUrl(), mTab.isIncognito());
        }
        if ((flags & InvalidateTypes.TITLE) != 0) {
            // Update cached title then notify observers.
            mTab.updateTitle();
        }
        if ((flags & InvalidateTypes.URL) != 0) {
            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) observers.next().onUrlUpdated(mTab);
        }
        if (mDelegate == null) {
            return;
        }
        mDelegate.navigationStateChanged(flags);
    }

    @Override
    public void visibleSSLStateChanged() {
        PolicyAuditor auditor = AppHooks.get().getPolicyAuditor();
        auditor.notifyCertificateFailure(
                PolicyAuditorJni.get().getCertificateFailure(mTab.getWebContents()),
                ContextUtils.getApplicationContext());
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) observers.next().onSSLStateUpdated(mTab);
        if (mDelegate == null) {
            return;
        }
        mDelegate.visibleSSLStateChanged();
    }

    @Override
    public boolean shouldCreateWebContents(GURL targetUrl) {
        if (mDelegate == null) {
            return false;
        }
        return mDelegate.shouldCreateWebContents(targetUrl);
    }

    @Override
    public void webContentsCreated(WebContents sourceWebContents, long openerRenderProcessId,
            long openerRenderFrameId, String frameName, GURL targetUrl,
            WebContents newWebContents) {
        if (mDelegate == null) {
            return;
        }
        mDelegate.webContentsCreated(sourceWebContents, openerRenderProcessId, openerRenderFrameId,
                frameName, targetUrl, newWebContents);
    }

    @Override
    public void showRepostFormWarningDialog() {
        if (mDelegate == null) {
            return;
        }
        mDelegate.showRepostFormWarningDialog();
    }

    @Override
    public boolean shouldBlockMediaRequest(GURL url) {
        if (mDelegate == null) {
            return false;
        }
        return mDelegate.shouldBlockMediaRequest(url);
    }

    @Override
    public void rendererUnresponsive() {
        if (mTab.getWebContents() != null) {
            TabWebContentsDelegateAndroidImplJni.get().onRendererUnresponsive(
                    mTab.getWebContents());
        }
        mTab.handleRendererResponsiveStateChanged(false);
        if (mDelegate == null) {
            return;
        }
        mDelegate.rendererUnresponsive();
    }

    @Override
    public void rendererResponsive() {
        if (mTab.getWebContents() != null) {
            TabWebContentsDelegateAndroidImplJni.get().onRendererResponsive(mTab.getWebContents());
        }
        mTab.handleRendererResponsiveStateChanged(true);
        if (mDelegate == null) {
            return;
        }
        mDelegate.rendererResponsive();
    }

    @Override
    public void closeContents() {
        // Execute outside of callback, otherwise we end up deleting the native
        // objects in the middle of executing methods on them.
        mHandler.removeCallbacks(mCloseContentsRunnable);
        mHandler.post(mCloseContentsRunnable);
        if (mDelegate == null) {
            return;
        }
        mDelegate.closeContents();
    }

    @CalledByNative
    @Override
    public void setOverlayMode(boolean useOverlayMode) {
        if (mDelegate == null) {
            return;
        }
        mDelegate.setOverlayMode(useOverlayMode);
    }

    @CalledByNative
    @Override
    public boolean shouldEnableEmbeddedMediaExperience() {
        if (mDelegate == null) {
            return false;
        }
        return mDelegate.shouldEnableEmbeddedMediaExperience();
    }

    /**
     * @return web preferences for enabling Picture-in-Picture.
     */
    @CalledByNative
    @Override
    public boolean isPictureInPictureEnabled() {
        if (mDelegate == null) {
            return false;
        }
        return mDelegate.isPictureInPictureEnabled();
    }

    /**
     * @return Night mode enabled/disabled for this Tab. To be used to propagate
     *         the preferred color scheme to the renderer.
     */
    @CalledByNative
    @Override
    public boolean isNightModeEnabled() {
        if (mDelegate == null) {
            return false;
        }
        return mDelegate.isNightModeEnabled();
    }

    /**
     * @return web preference for force dark mode.
     */
    @CalledByNative
    @Override
    public boolean isForceDarkWebContentEnabled() {
        if (mDelegate == null) {
            return false;
        }
        return mDelegate.isForceDarkWebContentEnabled();
    }

    /**
     * Return true if app banners are to be permitted in this tab. May need to be overridden.
     * @return true if app banners are permitted, and false otherwise.
     */
    @CalledByNative
    @Override
    public boolean canShowAppBanners() {
        if (mDelegate == null) {
            return false;
        }
        return mDelegate.canShowAppBanners();
    }

    @CalledByNative
    private boolean isTabLargeEnoughForDesktopSite() {
        return TabUtils.isTabLargeEnoughForDesktopSite(mTab);
    }

    /**
     * @return the WebAPK manifest scope. This gives frames within the scope increased privileges
     * such as autoplaying media unmuted.
     */
    @CalledByNative
    @Override
    public String getManifestScope() {
        if (mDelegate == null) {
            return super.getManifestScope();
        }
        return mDelegate.getManifestScope();
    }

    /**
     * Checks if the associated tab is currently presented in the context of custom tabs.
     * @return true if this is currently a custom tab.
     */
    @CalledByNative
    @Override
    public boolean isCustomTab() {
        if (mDelegate == null) {
            return false;
        }
        return mDelegate.isCustomTab();
    }

    @Override
    public int getTopControlsHeight() {
        if (mDelegate == null) {
            return 0;
        }
        return mDelegate.getTopControlsHeight();
    }

    @Override
    public int getTopControlsMinHeight() {
        if (mDelegate == null) {
            return 0;
        }
        return mDelegate.getTopControlsMinHeight();
    }

    @Override
    public int getBottomControlsHeight() {
        if (mDelegate == null) {
            return 0;
        }
        return mDelegate.getBottomControlsHeight();
    }

    @Override
    public int getBottomControlsMinHeight() {
        if (mDelegate == null) {
            return 0;
        }
        return mDelegate.getBottomControlsMinHeight();
    }

    @Override
    public boolean shouldAnimateBrowserControlsHeightChanges() {
        if (mDelegate == null) {
            return false;
        }
        return mDelegate.shouldAnimateBrowserControlsHeightChanges();
    }

    @Override
    public boolean controlsResizeView() {
        if (mDelegate == null) {
            return false;
        }
        return mDelegate.controlsResizeView();
    }

    @VisibleForTesting
    void showFramebustBlockInfobarForTesting(String url) {
        TabWebContentsDelegateAndroidImplJni.get().showFramebustBlockInfoBar(
                mTab.getWebContents(), url);
    }

    @NativeMethods
    public interface Natives {
        void onRendererUnresponsive(WebContents webContents);
        void onRendererResponsive(WebContents webContents);
        void showFramebustBlockInfoBar(WebContents webContents, String url);
    }
}
