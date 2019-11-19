// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Handler;

import androidx.annotation.CallSuper;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.findinpage.FindMatchRectsDetails;
import org.chromium.chrome.browser.findinpage.FindNotificationDetails;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.media.MediaCaptureNotificationService;
import org.chromium.chrome.browser.policy.PolicyAuditor;
import org.chromium.chrome.browser.policy.PolicyAuditorJni;
import org.chromium.chrome.browser.webapps.WebDisplayMode;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.content_public.browser.InvalidateTypes;
import org.chromium.content_public.browser.WebContents;

/**
 * A basic {@link WebContentsDelegateAndroid} that proxies methods into Tab. Forwards
 * some calls to the registered {@link TabObserver}.
 */
public abstract class TabWebContentsDelegateAndroid extends WebContentsDelegateAndroid {
    protected final Tab mTab;
    protected Handler mHandler;
    private final Runnable mCloseContentsRunnable;

    public TabWebContentsDelegateAndroid(Tab tab) {
        mTab = tab;
        mHandler = new Handler();
        mCloseContentsRunnable = () -> {
            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) observers.next().onCloseContents(mTab);
        };
    }

    @CalledByNative
    protected @WebDisplayMode int getDisplayMode() {
        return WebDisplayMode.BROWSER;
    }

    @CalledByNative
    private void onFindResultAvailable(FindNotificationDetails result) {
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) observers.next().onFindResultAvailable(result);
    }

    @Override
    public boolean addMessageToConsole(int level, String message, int lineNumber, String sourceId) {
        // Only output console.log messages on debug variants of Android OS. crbug/869804
        return !BuildInfo.isDebugAndroid();
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
    private static FindNotificationDetails createFindNotificationDetails(
            int numberOfMatches, Rect rendererSelectionRect,
            int activeMatchOrdinal, boolean finalUpdate) {
        return new FindNotificationDetails(numberOfMatches, rendererSelectionRect,
                activeMatchOrdinal, finalUpdate);
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
    public void loadingStateChanged(boolean toDifferentDocument) {
        boolean isLoading = mTab.getWebContents() != null && mTab.getWebContents().isLoading();
        if (isLoading) {
            mTab.onLoadStarted(toDifferentDocument);
        } else {
            mTab.onLoadStopped();
        }
    }

    @Override
    public void onUpdateUrl(String url) {
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) observers.next().onUpdateUrl(mTab, url);
    }

    @Override
    public void enterFullscreenModeForTab(boolean prefersNavigationBar) {
        mTab.enterFullscreenMode(new FullscreenOptions(prefersNavigationBar));
    }

    @Override
    public void exitFullscreenModeForTab() {
        mTab.exitFullscreenMode();
    }

    @Override
    public void navigationStateChanged(int flags) {
        if ((flags & InvalidateTypes.TAB) != 0) {
            MediaCaptureNotificationService.updateMediaNotificationForTab(
                    ContextUtils.getApplicationContext(), mTab.getId(), mTab.getWebContents(),
                    mTab.getUrl());
        }
        if ((flags & InvalidateTypes.TITLE) != 0) {
            // Update cached title then notify observers.
            mTab.updateTitle();
        }
        if ((flags & InvalidateTypes.URL) != 0) {
            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) {
                observers.next().onUrlUpdated(mTab);
            }
        }
    }

    @Override
    public void visibleSSLStateChanged() {
        PolicyAuditor auditor = AppHooks.get().getPolicyAuditor();
        auditor.notifyCertificateFailure(
                PolicyAuditorJni.get().getCertificateFailure(mTab.getWebContents()),
                ContextUtils.getApplicationContext());
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) {
            observers.next().onSSLStateUpdated(mTab);
        }
    }

    @CallSuper
    @Override
    public void webContentsCreated(WebContents sourceWebContents, long openerRenderProcessId,
            long openerRenderFrameId, String frameName, String targetUrl,
            WebContents newWebContents) {
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) {
            observers.next().webContentsCreated(mTab, sourceWebContents, openerRenderProcessId,
                    openerRenderFrameId, frameName, targetUrl, newWebContents);
        }
    }

    @CallSuper
    @Override
    public void rendererUnresponsive() {
        super.rendererUnresponsive();
        if (mTab.getWebContents() != null) {
            TabWebContentsDelegateAndroidJni.get().onRendererUnresponsive(mTab.getWebContents());
        }
        mTab.handleRendererResponsiveStateChanged(false);
    }

    @CallSuper
    @Override
    public void rendererResponsive() {
        super.rendererResponsive();
        if (mTab.getWebContents() != null) {
            TabWebContentsDelegateAndroidJni.get().onRendererResponsive(mTab.getWebContents());
        }
        mTab.handleRendererResponsiveStateChanged(true);
    }

    /**
     * Returns whether the page should resume accepting requests for the new window. This is
     * used when window creation is asynchronous and the navigations need to be delayed.
     */
    @CalledByNative
    protected abstract boolean shouldResumeRequestsForCreatedWindow();

    /**
     * Creates a new tab with the already-created WebContents. The tab for the added
     * contents should be reparented correctly when this method returns.
     * @param sourceWebContents Source WebContents from which the new one is created.
     * @param webContents Newly created WebContents object.
     * @param disposition WindowOpenDisposition indicating how the tab should be created.
     * @param initialPosition Initial position of the content to be created.
     * @param userGesture {@code true} if opened by user gesture.
     * @return {@code true} if new tab was created successfully with a give WebContents.
     */
    @CalledByNative
    protected abstract boolean addNewContents(WebContents sourceWebContents,
            WebContents webContents, int disposition, Rect initialPosition, boolean userGesture);

    @Override
    public void closeContents() {
        // Execute outside of callback, otherwise we end up deleting the native
        // objects in the middle of executing methods on them.
        mHandler.removeCallbacks(mCloseContentsRunnable);
        mHandler.post(mCloseContentsRunnable);
    }

    /**
     * Sets the overlay mode.
     * Overlay mode means that we are currently using AndroidOverlays to display video, and
     * that the compositor's surface should support alpha and not be marked as opaque.
     */
    @CalledByNative
    protected abstract void setOverlayMode(boolean useOverlayMode);

    /**
     *  This is currently called when committing a pre-rendered page or activating a portal.
     */
    @CalledByNative
    private void swapWebContents(
            WebContents webContents, boolean didStartLoad, boolean didFinishLoad) {
        mTab.swapWebContents(webContents, didStartLoad, didFinishLoad);
    }

    private float getDipScale() {
        return mTab.getWindowAndroid().getDisplay().getDipScale();
    }

    public void showFramebustBlockInfobarForTesting(String url) {
        TabWebContentsDelegateAndroidJni.get().showFramebustBlockInfoBar(
                mTab.getWebContents(), url);
    }

    /**
     * Provides info on web preferences for viewing downloaded media.
     * @return enabled Whether embedded media experience should be enabled.
     */
    @CalledByNative
    protected boolean shouldEnableEmbeddedMediaExperience() {
        return false;
    }

    /**
     * @return web preferences for enabling Picture-in-Picture.
     */
    @CalledByNative
    protected boolean isPictureInPictureEnabled() {
        return false;
    }

    /**
     * @return Night mode enabled/disabled for this Tab. To be used to propagate
     *         the preferred color scheme to the renderer.
     */
    @CalledByNative
    protected boolean isNightModeEnabled() {
        return false;
    }

    /**
     * Return true if app banners are to be permitted in this tab. May need to be overridden.
     * @return true if app banners are permitted, and false otherwise.
     */
    @CalledByNative
    protected boolean canShowAppBanners() {
        return true;
    }

    /**
     * @return the Webapp manifest scope, which is used to allow frames within the scope to
     *         autoplay media unmuted.
     */
    @CalledByNative
    protected String getManifestScope() {
        return null;
    }

    /**
     * Checks if the associated tab is currently presented in the context of custom tabs.
     * @return true if this is currently a custom tab.
     */
    @CalledByNative
    protected boolean isCustomTab() {
        return false;
    }

    @NativeMethods
    interface Natives {
        void onRendererUnresponsive(WebContents webContents);
        void onRendererResponsive(WebContents webContents);
        void showFramebustBlockInfoBar(WebContents webContents, String url);
    }
}
