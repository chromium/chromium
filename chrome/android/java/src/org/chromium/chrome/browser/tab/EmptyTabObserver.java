// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.graphics.Bitmap;
import android.view.ContextMenu;

import org.chromium.chrome.browser.findinpage.FindMatchRectsDetails;
import org.chromium.chrome.browser.findinpage.FindNotificationDetails;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.BrowserControlsState;
import org.chromium.net.NetError;

/**
 * An implementation of the {@link TabObserver} which has empty implementations of all methods.
 *
 * Note: Do not replace this with TabObserver with default interface methods as it inadvertently
 * bloats the number of methods. See https://crbug.com/781359.
 */
public class EmptyTabObserver implements TabObserver {
    @Override
    public void onInitialized(Tab tab, TabState tabState) {}

    @Override
    public void onShown(Tab tab, @TabSelectionType int type) {}

    @Override
    public void onHidden(Tab tab, @TabHidingType int reason) {}

    @Override
    public void onClosingStateChanged(Tab tab, boolean closing) { }

    @Override
    public void onDestroyed(Tab tab) { }

    @Override
    public void onContentChanged(Tab tab) { }

    @Override
    public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) { }

    @Override
    public void onPageLoadStarted(Tab tab, String url) { }

    @Override
    public void onPageLoadFinished(Tab tab, String url) {}

    @Override
    public void onPageLoadFailed(Tab tab, @NetError int errorCode) {}

    @Override
    public void onRestoreStarted(Tab tab) {}

    @Override
    public void onFaviconUpdated(Tab tab, Bitmap icon) { }

    @Override
    public void onTitleUpdated(Tab tab) { }

    @Override
    public void onUrlUpdated(Tab tab) { }

    @Override
    public void onSSLStateUpdated(Tab tab) { }

    @Override
    public void onCrash(Tab tab) {}

    @Override
    public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) { }

    @Override
    public void onContextMenuShown(Tab tab, ContextMenu menu) { }

    @Override
    public void onContextualActionBarVisibilityChanged(Tab tab, boolean visible) { }

    @Override
    public void onCloseContents(Tab tab) {}

    @Override
    public void onLoadStarted(Tab tab, boolean toDifferentDocument) { }

    @Override
    public void onLoadStopped(Tab tab, boolean toDifferentDocument) { }

    @Override
    public void onLoadProgressChanged(Tab tab, float progress) {}

    @Override
    public void onUpdateUrl(Tab tab, String url) { }

    @Override
    public void onEnterFullscreenMode(Tab tab, FullscreenOptions options) {}

    @Override
    public void onExitFullscreenMode(Tab tab) {}

    @Override
    public void onDidFailLoad(
            Tab tab, boolean isMainFrame, int errorCode, String description, String failingUrl) {}

    @Override
    public void onDidStartNavigation(Tab tab, NavigationHandle navigationHandle) {}

    @Override
    public void onDidRedirectNavigation(Tab tab, NavigationHandle navigationHandle) {}

    @Override
    public void onDidFinishNavigation(Tab tab, NavigationHandle navigationHandle) {}

    @Override
    public void didFirstVisuallyNonEmptyPaint(Tab tab) {}

    @Override
    public void onDidChangeThemeColor(Tab tab, int color) { }

    @Override
    public void onDidAttachInterstitialPage(Tab tab) { }

    @Override
    public void onDidDetachInterstitialPage(Tab tab) { }

    @Override
    public void onBackgroundColorChanged(Tab tab, int color) { }

    @Override
    public void webContentsCreated(Tab tab, WebContents sourceWebContents,
            long openerRenderProcessId, long openerRenderFrameId, String frameName,
            String targetUrl, WebContents newWebContents) {}

    @Override
    public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {}

    @Override
    public void onInteractabilityChanged(boolean isInteractable) {}

    @Override
    public void onRendererResponsiveStateChanged(Tab tab, boolean isResponsive) {}

    @Override
    public void onNavigationEntriesDeleted(Tab tab) {}

    @Override
    public void onBrowserControlsConstraintsUpdated(
            Tab tab, @BrowserControlsState int constraints) {}

    @Override
    public void onFindResultAvailable(FindNotificationDetails result) {}

    @Override
    public void onFindMatchRectsAvailable(FindMatchRectsDetails result) {}

    @Override
    public void onRootIdChanged(Tab tab, int newRootId) {}

    @Override
    public void onBrowserControlsOffsetChanged(
            Tab tab, int topControlsOffsetY, int bottomControlsOffsetY, int contentOffsetY) {}
}
