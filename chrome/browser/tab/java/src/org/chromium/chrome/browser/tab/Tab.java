// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.UserDataHost;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Tab is a visual/functional unit that encapsulates the content (not just web site content
 * from network but also other types of content such as NTP, navigation history, etc) and
 * presents it to users who perceive it as one of the 'pages' managed by Chrome.
 */
public interface Tab extends TabLifecycle {
    public static final int INVALID_TAB_ID = -1;

    @IntDef({TabLoadStatus.PAGE_LOAD_FAILED, TabLoadStatus.DEFAULT_PAGE_LOAD})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabLoadStatus {
        int PAGE_LOAD_FAILED = 0;
        int DEFAULT_PAGE_LOAD = 1;
    }

    /**
     * Adds a {@link TabObserver} to be notified on {@link Tab} changes.
     * @param observer The {@link TabObserver} to add.
     */
    void addObserver(TabObserver observer);

    /**
     * Removes a {@link TabObserver}.
     * @param observer The {@link TabObserver} to remove.
     */
    void removeObserver(TabObserver observer);

    /** Returns if the given {@link TabObserver} is present. */
    boolean hasObserver(TabObserver observer);

    /**
     * @return {@link UserDataHost} that manages {@link UserData} objects attached to.
     *         This is used for managing Tab-specific attributes/objects without Tab
     *         object having to know about them directly.
     */
    UserDataHost getUserDataHost();

    /**
     * @return The web contents associated with this tab.
     */
    @Nullable
    WebContents getWebContents();

    /**
     * @return The {@link Activity} {@link Context} if this {@link Tab} is attached to an
     *         {@link Activity}, otherwise the themed application context (e.g. hidden tab or
     *         browser action tab).
     */
    @NonNull
    Context getContext();

    /**
     * @return The {@link WindowAndroid} associated with this {@link Tab}.
     */
    WindowAndroid getWindowAndroid();

    /**
     * Update the attachment state to Window(Activity).
     * @param window A new {@link WindowAndroid} to attach the tab to. If {@code null},
     *        the tab is being detached. See {@link ReparentingTask#detach()} for details.
     * @param tabDelegateFactory The new delegate factory this tab should be using. Can be
     *        {@code null} even when {@code window} is not, meaning we simply want to swap out
     *        {@link WindowAndroid} for this tab and keep using the current delegate factory.
     */
    void updateAttachment(
            @Nullable WindowAndroid window, @Nullable TabDelegateFactory tabDelegateFactory);

    /**
     * @return Content view used for rendered web contents. Can be null
     *    if web contents is null.
     */
    ContentView getContentView();

    /**
     * @return The {@link View} displaying the current page in the tab. This can be {@code null}, if
     *         the tab is frozen or being initialized or destroyed.
     */
    View getView();

    /**
     * @return The {@link TabViewManager} that is responsible for managing custom {@link View}s
     * shown on top of content in this Tab.
     */
    TabViewManager getTabViewManager();

    /**
     * @return The id representing this tab.
     */
    int getId();

    /**
     * @return Parameters that should be used for a lazily loaded Tab.  May be null.
     */
    LoadUrlParams getPendingLoadParams();

    /**
     * @return The URL that is loaded in the current tab. This may not be the same as
     *         the last committed URL if a new navigation is in progress.
     */
    GURL getUrl();

    /**
     * @return Original url of the tab without any Chrome feature modifications applied
     *         (e.g. reader mode).
     */
    GURL getOriginalUrl();

    /**
     * @return The tab title.
     */
    String getTitle();

    /**
     * @return The {@link NativePage} associated with the current page, or {@code null} if there is
     *         no current page or the current page is displayed using something besides
     *         {@link NativePage}.
     */
    NativePage getNativePage();

    /**
     * @return Whether or not the {@link Tab} represents a {@link NativePage}.
     */
    boolean isNativePage();

    /**
     * @return Whether a custom view shown through {@link TabViewManager} is being displayed instead
     * of the current WebContents.
     */
    boolean isShowingCustomView();

    /**
     * Replaces the current NativePage with a empty stand-in for a NativePage. This can be used
     * to reduce memory pressure.
     */
    void freezeNativePage();

    /**
     * @return The reason the Tab was launched (from a link, external app, etc).
     *         May change over time, for instance, to {@code FROM_RESTORE} during
     *         tab restoration.
     */
    @TabLaunchType
    int getLaunchType();

    /**
     * @return The theme color for this tab.
     */
    int getThemeColor();

    /**
     * @return {@code true} if the theme color from contents is valid and can be used for theming.
     */
    boolean isThemingAllowed();

    /**
     * @return {@code true} if the Tab is in incognito mode.
     */
    boolean isIncognito();

    /**
     * @return Whether the {@link Tab} is currently showing an error page.
     */
    boolean isShowingErrorPage();

    /**
     * @return true iff the tab doesn't hold a live page. For example, this could happen
     *         when the tab holds frozen WebContents state that is yet to be inflated.
     */
    boolean isFrozen();

    /**
     * @return Whether the tab can currently be interacted with by the user.  This requires the
     *         view owned by the Tab to be visible and in a state where the user can interact
     *         with it (i.e. not in something like the phone tab switcher).
     */
    boolean isUserInteractable();

    /**
     *  Sets Parent for the current Tab and other tab related parent properties.
     */

    void reparentTab(Tab parent);

    /**
     * Causes this tab to navigate to the specified URL.
     * @param params parameters describing the url load. Note that it is important to set correct
     *         page transition as it is used for ranking URLs in the history so the omnibox
     *         can report suggestions correctly.
     * @return PAGE_LOAD_FAILED if the URL could not be loaded, otherwise DEFAULT_PAGE_LOAD.
     */
    int loadUrl(LoadUrlParams params);

    /**
     * Loads the tab if it's not loaded (e.g. because it was killed in background).
     * This will trigger a regular load for tabs with pending lazy first load (tabs opened in
     * background on low-memory devices).
     * @param caller The caller of this method.
     * @return true iff the Tab handled the request.
     */
    boolean loadIfNeeded(int caller);

    /**
     * Reloads the current page content.
     */
    void reload();

    /**
     * Reloads the current page content.
     * This version ignores the cache and reloads from the network.
     */
    void reloadIgnoringCache();

    /**
     * Stop the current navigation.
     */
    void stopLoading();

    /**
     * @return Whether the Tab has requested a reload.
     */
    boolean needsReload();

    /**
     * @return true iff the tab is loading and an interstitial page is not showing.
     */
    boolean isLoading();

    /**
     * @return true iff the tab is performing a restore page load.
     */
    boolean isBeingRestored();

    /**
     * @return a value between 0 and 100 reflecting what percentage of the page load is complete.
     */
    float getProgress();

    /**
     * @return Whether or not this tab has a previous navigation entry.
     */
    boolean canGoBack();

    /**
     * @return Whether or not this tab has a navigation entry after the current one.
     */
    boolean canGoForward();

    /**
     * Goes to the navigation entry before the current one.
     */
    void goBack();

    /**
     * Goes to the navigation entry after the current one.
     */
    void goForward();

    /**
     * Set whether {@link Tab} metadata (specifically all {@link PersistedTabData})
     * will be saved. Not all Tabs need to be persisted across restarts.
     * The default value when a Tab is initialized is false.
     */
    void setIsTabSaveEnabled(boolean isSaveEnabled);

    /**
     * @return true if the {@link Tab} is a custom tab.
     */
    boolean isCustomTab();
}
