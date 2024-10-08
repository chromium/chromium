// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.cc.input.BrowserControlsOffsetTagsInfo;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.components.find_in_page.FindMatchRectsDetails;
import org.chromium.components.find_in_page.FindNotificationDetails;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.net.NetError;
import org.chromium.ui.mojom.VirtualKeyboardMode;
import org.chromium.url.GURL;

/**
 * An implementation of the {@link TabObserver} which has empty implementations of all methods.
 *
 * Note: Do not replace this with TabObserver with default interface methods as it inadvertently
 * bloats the number of methods. See https://crbug.com/781359.
 */
public class EmptyTabObserver implements TabObserver {
    @Override
    public void onInitialized(Tab tab, String appId) {}

    @Override
    public void onShown(Tab tab, @TabSelectionType int type) {}

    @Override
    public void onHidden(Tab tab, @TabHidingType int reason) {}

    @Override
    public void onClosingStateChanged(Tab tab, boolean closing) {}

    @Override
    public void onDestroyed(Tab tab) {}

    @Override
    public void onContentChanged(Tab tab) {}

    @Override
    public void onLoadUrl(Tab tab, LoadUrlParams params, LoadUrlResult loadUrlResult) {}

    @Override
    public void onPageLoadStarted(Tab tab, GURL url) {}

    @Override
    public void onPageLoadFinished(Tab tab, GURL url) {}

    @Override
    public void onPageLoadFailed(Tab tab, @NetError int errorCode) {}

    @Override
    public void onRestoreStarted(Tab tab) {}

    @Override
    public void onRestoreFailed(Tab tab) {}

    @Override
    public void onFaviconUpdated(Tab tab, Bitmap icon, GURL iconUrl) {}

    @Override
    public void onTitleUpdated(Tab tab) {}

    @Override
    public void onUrlUpdated(Tab tab) {}

    @Override
    public void onSSLStateUpdated(Tab tab) {}

    @Override
    public void onCrash(Tab tab) {}

    @Override
    public void webContentsWillSwap(Tab tab) {}

    @Override
    public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {}

    @Override
    public void onContextMenuShown(Tab tab) {}

    @Override
    public void onCloseContents(Tab tab) {}

    @Override
    public void onLoadStarted(Tab tab, boolean toDifferentDocument) {}

    @Override
    public void onLoadStopped(Tab tab, boolean toDifferentDocument) {}

    @Override
    public void onLoadProgressChanged(Tab tab, float progress) {}

    @Override
    public void onUpdateUrl(Tab tab, GURL url) {}

    @Override
    public void onDidStartNavigationInPrimaryMainFrame(
            Tab tab, NavigationHandle navigationHandle) {}

    @Override
    public void onDidRedirectNavigation(Tab tab, NavigationHandle navigationHandle) {}

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(
            Tab tab, NavigationHandle navigationHandle) {}

    @Override
    public void onDidFinishNavigationEnd() {}

    @Override
    public void didFirstVisuallyNonEmptyPaint(Tab tab) {}

    @Override
    public void onDidChangeThemeColor(Tab tab, int color) {}

    @Override
    public void onBackgroundColorChanged(Tab tab, int color) {}

    @Override
    public void onVirtualKeyboardModeChanged(Tab tab, @VirtualKeyboardMode.EnumType int mode) {}

    @Override
    public void onInteractabilityChanged(Tab tab, boolean isInteractable) {}

    @Override
    public void onRendererResponsiveStateChanged(Tab tab, boolean isResponsive) {}

    @Override
    public void onNavigationEntriesAppended(Tab tab) {}

    @Override
    public void onNavigationEntriesDeleted(Tab tab) {}

    @Override
    public void onFindResultAvailable(FindNotificationDetails result) {}

    @Override
    public void onFindMatchRectsAvailable(FindMatchRectsDetails result) {}

    @Override
    public void onBrowserControlsOffsetChanged(
            Tab tab,
            int topControlsOffsetY,
            int bottomControlsOffsetY,
            int contentOffsetY,
            int topControlsMinHeightOffsetY,
            int bottomControlsMinHeightOffsetY) {}

    @Override
    public void onBrowserControlsConstraintsChanged(
            Tab tab,
            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
            BrowserControlsOffsetTagsInfo offsetTagsInfo,
            @BrowserControlsState int constraints) {}

    @Override
    public void onWillShowBrowserControls(Tab tab, boolean viewTransitionOptIn) {}

    @Override
    public void onContentViewScrollingStateChanged(boolean scrolling) {}

    @Override
    public void onNavigationStateChanged() {}

    @Override
    public void onDidChangeCloseSignalInterceptStatus() {}

    @Override
    public void onRootIdChanged(Tab tab, int newRootId) {}

    @Override
    public void onTabGroupIdChanged(Tab tab, @Nullable Token tabGroupId) {}

    @Override
    public void onGestureBegin() {}

    @Override
    public void onGestureEnd() {}

    @Override
    public void onTabContentSensitivityChanged(Tab tab, boolean contentIsSensitive) {}
}
