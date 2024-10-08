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
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.mojom.VirtualKeyboardMode;
import org.chromium.url.GURL;

/** An observer that is notified of changes to a {@link Tab} object. */
public interface TabObserver {
    /**
     * Called when a {@link Tab} finished initialization. The {@link TabState} contains,
     * if not {@code null}, various states that a Tab should restore itself from.
     * @param tab The notifying {@link Tab}.
     * @param appId ID of the external app that opened this tab.
     */
    void onInitialized(Tab tab, String appId);

    /**
     * Called when a {@link Tab} is shown.
     * @param tab The notifying {@link Tab}.
     * @param type Specifies how the tab was selected.
     */
    void onShown(Tab tab, @TabSelectionType int type);

    /**
     * Called when a {@link Tab} is hidden.
     * @param tab The notifying {@link Tab}.
     * @param type Specifies how the tab was hidden.
     */
    void onHidden(Tab tab, @TabHidingType int type);

    /**
     * Called when a {@link Tab}'s closing state has changed.
     *
     * @param tab The notifying {@link Tab}.
     * @param closing Whether the {@link Tab} is currently marked for closure.
     */
    void onClosingStateChanged(Tab tab, boolean closing);

    /**
     * Called when a {@link Tab} is being destroyed.
     * @param tab The notifying {@link Tab}.
     */
    void onDestroyed(Tab tab);

    /**
     * Called when the tab content changes (to/from native pages or swapping native WebContents).
     * @param tab The notifying {@link Tab}.
     */
    void onContentChanged(Tab tab);

    /**
     * Called when loadUrl is triggered on a a {@link Tab}.
     *
     * @param tab The notifying {@link Tab}.
     * @param params The params describe the page being loaded.
     * @param loadUrlResult The result of the loadUrl.
     */
    void onLoadUrl(Tab tab, LoadUrlParams params, LoadUrlResult loadUrlResult);

    /**
     * Called when a tab has started to load a page.
     * <p>
     * This will occur when the main frame starts the navigation, and will also occur in instances
     * where we need to simulate load progress (i.e. swapping in a not fully loaded pre-rendered
     * page).
     * <p>
     * For visual loading indicators/throbbers, {@link #onLoadStarted(Tab)} and
     * {@link #onLoadStopped(Tab)} should be used to drive updates.
     *  @param tab The notifying {@link Tab}.
     * @param url The committed URL being navigated to.
     */
    void onPageLoadStarted(Tab tab, GURL url);

    /**
     * Called when a tab has finished loading a page.
     *  @param tab The notifying {@link Tab}.
     * @param url The committed URL that was navigated to.
     */
    void onPageLoadFinished(Tab tab, GURL url);

    /**
     * Called when a tab has failed loading a page.
     *
     * @param tab The notifying {@link Tab}.
     * @param errorCode The error code that causes the page to fail loading.
     */
    void onPageLoadFailed(Tab tab, int errorCode);

    /**
     * Called when the favicon of a {@link Tab} has been updated.
     * @param tab The notifying {@link Tab}.
     * @param icon The favicon that was received.
     * @param iconUrl The URL that the icon was fetched from.
     */
    void onFaviconUpdated(Tab tab, Bitmap icon, GURL iconUrl);

    /**
     * Called when the title of a {@link Tab} changes.
     * @param tab The notifying {@link Tab}.
     */
    void onTitleUpdated(Tab tab);

    /**
     * Called when the URL of a {@link Tab} changes.
     * @param tab The notifying {@link Tab}.
     */
    void onUrlUpdated(Tab tab);

    /**
     * Called when the SSL state of a {@link Tab} changes.
     * @param tab The notifying {@link Tab}.
     */
    void onSSLStateUpdated(Tab tab);

    /**
     * Called when the ContentView of a {@link Tab} crashes.
     * @param tab The notifying {@link Tab}.
     */
    void onCrash(Tab tab);

    /**
     * Called when restore of the corresponding tab is triggered.
     * @param tab The notifying {@link Tab}.
     */
    void onRestoreStarted(Tab tab);

    /**
     * Called when restoration of the corresponding tab failed.
     * @param tab The notifying {@link Tab}.
     */
    void onRestoreFailed(Tab tab);

    /**
     * Called when the WebContents of a {@link Tab} is about to be swapped.
     * @param tab The notifying {@link Tab}
     */
    void webContentsWillSwap(Tab tab);

    /**
     * Called when the WebContents of a {@link Tab} have been swapped.
     * @param tab The notifying {@link Tab}.
     * @param didStartLoad Whether WebContentsObserver::DidStartProvisionalLoadForFrame() has
     *     already been called.
     * @param didFinishLoad Whether WebContentsObserver::DidFinishLoad() has already been called.
     */
    void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad);

    /**
     * Called when a context menu is shown for a {@link WebContents} owned by a {@link Tab}.
     * @param tab  The notifying {@link Tab}.
     */
    void onContextMenuShown(Tab tab);

    // WebContentsDelegateAndroid methods ---------------------------------------------------------

    /**
     * Called when the WebContents is closed.
     * @param tab The notifying {@link Tab}.
     */
    void onCloseContents(Tab tab);

    /**
     * Called when the WebContents starts loading. Different from
     * {@link #onPageLoadStarted(Tab, GURL)}, if the user is navigated to a different url while
     * staying in the same html document, {@link #onLoadStarted(Tab)} will be called, while
     * {@link #onPageLoadStarted(Tab, GURL)} will not.
     * @param tab The notifying {@link Tab}.
     * @param toDifferentDocument Whether this navigation will transition between
     * documents (i.e., not a fragment navigation or JS History API call).
     */
    void onLoadStarted(Tab tab, boolean toDifferentDocument);

    /**
     * Called when the contents loading stops.
     * @param tab The notifying {@link Tab}.
     */
    void onLoadStopped(Tab tab, boolean toDifferentDocument);

    /**
     * Called when the load progress of a {@link Tab} changes.
     * @param tab      The notifying {@link Tab}.
     * @param progress The new progress from [0,1].
     */
    void onLoadProgressChanged(Tab tab, float progress);

    /**
     * Called when the URL of a {@link Tab} changes.
     * @param tab The notifying {@link Tab}.
     * @param url The new URL.
     */
    void onUpdateUrl(Tab tab, GURL url);

    // WebContentsObserver methods ---------------------------------------------------------

    /**
     * Called when a navigation in the primary main frame is started in the WebContents.
     * @param tab The notifying {@link Tab}.
     * @param navigationHandle Pointer to a NavigationHandle representing the navigation.
     *                         Its lifetime end at the end of onDidFinishNavigation().
     */
    void onDidStartNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigationHandle);

    /**
     * TODO(crbug.com/40264745) Temporary fix for LocationBarModel not properly caching same
     * document navigation state. Will be removed later, see bug for more details.
     */
    void onDidFinishNavigationEnd();

    /**
     * Called when a navigation is redirected in the WebContents.
     * @param tab The notifying {@link Tab}.
     * @param navigationHandle Pointer to a NavigationHandle representing the navigation.
     *                         Its lifetime end at the end of onDidFinishNavigation().
     */
    void onDidRedirectNavigation(Tab tab, NavigationHandle navigationHandle);

    /**
     * Called when a navigation is finished i.e. committed, aborted or replaced by a new one, in the
     * primary main frame.
     * @param tab The notifying {@link Tab}.
     * @param navigationHandle Pointer to a NavigationHandle representing the navigation.
     *                         Its lifetime end at the end of this function.
     */
    void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation);

    /**
     * Called when the page has painted something non-empty.
     * @param tab The notifying {@link Tab}.
     */
    void didFirstVisuallyNonEmptyPaint(Tab tab);

    /**
     * Called when the theme color is changed
     * @param tab   The notifying {@link Tab}.
     * @param color the new color in ARGB format.
     */
    void onDidChangeThemeColor(Tab tab, int color);

    /**
     * Called when the background color for the tab has changed.
     * @param tab The notifying {@link Tab}.
     * @param color The current background color.
     */
    void onBackgroundColorChanged(Tab tab, int color);

    /**
     * Called when the virtual keyboard mode in the tab's current page has been changed.
     * @param tab The notifying {@link Tab}.
     * @param mode The current virtual keyboard mode.
     */
    void onVirtualKeyboardModeChanged(Tab tab, @VirtualKeyboardMode.EnumType int mode);

    /**
     * Called when the Tab is attached or detached from an {@code Activity}. By default, this will
     * automatically unregister the tab observer if the Tab is detached from the window.
     *
     * TabObservers that are scoped to the Tab itself (either by direct ownership or through
     * UserData) will need to override this behavior. To do so, ensure there's a functional hook to
     * unregister the TabObserver to prevent leaking. When overriding this, keep in mind that tabs
     * can outlive the activity in some cases (change of theme, changing from phone/tablet,
     * multi-window, etc).
     *
     * @param tab The notifying {@link Tab}.
     * @param window {@link WindowAndroid} which the Tab is being associated with. {@code null} if
     *         the tab is being detached.
     */
    default void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        if (tab == null || window != null) return;
        tab.removeObserver(this);
    }

    /**
     * A notification when tab changes whether or not it is interactable and is accepting input.
     * @param tab The notifying {@link Tab}.
     * @param isInteractable Whether or not the tab is interactable.
     */
    void onInteractabilityChanged(Tab tab, boolean isInteractable);

    /**
     * Called when renderer changes its state about being responsive to requests.
     *
     * @param tab The notifying {@link Tab}.
     * @param {@code true} if the renderer becomes responsive, otherwise {@code false}.
     */
    void onRendererResponsiveStateChanged(Tab tab, boolean isResponsive);

    /**
     * Called when navigation entries of a tab have been appended while the tab is frozen.
     *
     * @param tab The notifying {@link Tab}.
     */
    void onNavigationEntriesAppended(Tab tab);

    /**
     * Called when navigation entries of a tab have been deleted.
     *
     * @param tab The notifying {@link Tab}.
     */
    void onNavigationEntriesDeleted(Tab tab);

    /**
     * Called when a find result is received.
     * @param result Detail information on the find result.
     */
    void onFindResultAvailable(FindNotificationDetails result);

    /**
     * Called when the rects corresponding to the find matches are received.
     * @param result Detail information on the matched rects.
     */
    void onFindMatchRectsAvailable(FindMatchRectsDetails result);

    /**
     * Called when offset values related with the browser controls have been changed by the
     * renderer.
     *
     * @param topControlsOffsetY The Y offset of the top controls in physical pixels.
     * @param bottomControlsOffsetY The Y offset of the bottom controls in physical pixels.
     * @param contentOffsetY The Y offset of the content in physical pixels.
     * @param topControlsMinHeightOffsetY The Y offset of the current top controls min-height.
     * @param bottomControlsMinHeightOffsetY The Y offset of the current bottom controls min-height.
     */
    void onBrowserControlsOffsetChanged(
            Tab tab,
            int topControlsOffsetY,
            int bottomControlsOffsetY,
            int contentOffsetY,
            int topControlsMinHeightOffsetY,
            int bottomControlsMinHeightOffsetY);

    /**
     * @see BrowserControlsStateProvider.onControlsConstraintsChanged
     */
    void onBrowserControlsConstraintsChanged(
            Tab tab,
            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
            BrowserControlsOffsetTagsInfo offsetTagsInfo,
            @BrowserControlsState int constraints);

    /**
     * Called when the tab is about to notify its renderer to show the browser controls.
     *
     * @param tab The notifying {@link Tab}.
     * @param tab Whether the current page has opted in to same-origin view transitions.
     */
    void onWillShowBrowserControls(Tab tab, boolean viewTransitionOptIn);

    /**
     * Called when scrolling state of Tab's content view changes.
     *
     * @param scrolling {@code true} if scrolling started; {@code false} if stopped.
     */
    void onContentViewScrollingStateChanged(boolean scrolling);

    /** Called when the gesture begin event is received. */
    void onGestureBegin();

    /** Called when the gesture end event is received. */
    void onGestureEnd();

    /** Back press refactor related. Called when navigation state is invalidated. */
    void onNavigationStateChanged();

    /**
     * CloseWatcher web API support. If the currently focused frame has a CloseWatcher registered in
     * JavaScript, the CloseWatcher should receive the next "close" operation, based on what the OS
     * convention for closing is. This function is called when the focused frame changes or a
     * CloseWatcher registered/unregistered to update whether the CloseWatcher should intercept.
     */
    void onDidChangeCloseSignalInterceptStatus();

    /**
     * Broadcast that the timestamp on a {@link Tab} has changed
     *
     * @param tab {@link Tab} timestamp has changed on
     * @param timestampMillis new value of the timestamp
     */
    default void onTimestampChanged(Tab tab, long timestampMillis) {}

    // TODO(crbug.com/41497290): deprecate RootId once TabGroupId has finished replacing it.
    /**
     * Broadcast that root identifier on a {@link Tab} has changed. This method will be functionally
     * replaced by onTabGroupIdChanged as part of https://crbug.com/1523745.
     *
     * @param tab {@link Tab} root identifier has changed on
     * @param newRootId new value of new root id
     */
    default void onRootIdChanged(Tab tab, int newRootId) {}

    /**
     * Broadcast that tab group ID on a {@link Tab} has changed.
     *
     * @param tab The {@link Tab} root identifier has changed on
     * @param tabGroupId The new tab group ID, may be null.
     */
    default void onTabGroupIdChanged(Tab tab, @Nullable Token tabGroupId) {}

    /**
     * Called when the animation state for the back forward session history navigation has changed.
     * Retrieve the current animation state using the Tab's WebContents.
     */
    default void didBackForwardTransitionAnimationChange() {}

    /** Called when the content sensitivity of the tab changes. */
    default void onTabContentSensitivityChanged(Tab tab, boolean contentIsSensitive) {}
}
