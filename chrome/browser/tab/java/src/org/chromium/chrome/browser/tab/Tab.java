// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Token;
import org.chromium.base.UserDataHost;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.tabs.DetachReason;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Tab is a visual/functional unit that encapsulates the content (not just web site content from
 * network but also other types of content such as NTP, navigation history, etc) and presents it to
 * users who perceive it as one of the 'pages' managed by Chrome.
 */
@NullMarked
public interface Tab extends TabLifecycle {
    @TabId int INVALID_TAB_ID = -1;
    long INVALID_TIMESTAMP = -1;

    @IntDef({TabLoadStatus.PAGE_LOAD_FAILED, TabLoadStatus.DEFAULT_PAGE_LOAD})
    @Retention(RetentionPolicy.SOURCE)
    @interface TabLoadStatus {
        int PAGE_LOAD_FAILED = 0;
        int DEFAULT_PAGE_LOAD = 1;
    }

    /** Tracks the media indicator state of the tab. */
    // LINT.IfChange(AndroidTabMediaState)
    @IntDef({
        MediaState.NONE,
        MediaState.MUTED,
        MediaState.AUDIBLE,
        MediaState.RECORDING,
        MediaState.SHARING,
        MediaState.MAX_VALUE,
        MediaState.COUNT,
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface MediaState {
        int NONE = 0;
        int MUTED = 1;
        int AUDIBLE = 2;
        int RECORDING = 3;
        int SHARING = 4;
        int MAX_VALUE = SHARING;
        int COUNT = MAX_VALUE + 1;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:AndroidTabMediaState)

    /** The result of the loadUrl. */
    class LoadUrlResult {
        /** Tab load status. */
        public final @TabLoadStatus int tabLoadStatus;

        /** NavigationHandle for the loaded url. */
        public final @Nullable NavigationHandle navigationHandle;

        @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
        public LoadUrlResult(
                @TabLoadStatus int tabLoadStatus, @Nullable NavigationHandle navigationHandle) {
            this.tabLoadStatus = tabLoadStatus;
            this.navigationHandle = navigationHandle;
        }
    }

    @FunctionalInterface
    interface SelectionStateSupplier {
        /**
         * @param tabId The ID of the tab to check.
         * @return True if the tab is selected.
         */
        boolean isTabMultiSelected(int tabId);
    }

    /**
     * Adds a {@link TabObserver} to be notified on {@link Tab} changes.
     *
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
     * Returns the {@link UserDataHost} that manages {@link UserData} objects attached to. This is
     * used for managing Tab-specific attributes/objects without Tab object having to know about
     * them directly.
     */
    UserDataHost getUserDataHost();

    /** Returns the Profile this tab is associated with. */
    Profile getProfile();

    /** Returns the web contents associated with this tab. */
    @Nullable WebContents getWebContents();

    /**
     * Returns the {@link Activity} {@link Context} if this {@link Tab} is attached to an {@link
     * Activity}, otherwise the themed application context (e.g. hidden tab or browser action tab).
     */
    Context getContext();

    /**
     * Returns the {@link WindowAndroid} associated with this {@link Tab}. May be null if the tab is
     * detached.
     */
    @Nullable WindowAndroid getWindowAndroid();

    /**
     * Returns the {@link WindowAndroid} associated with this {@link Tab}. Asserts that the {@link
     * WindowAndroid} is not null.
     */
    default WindowAndroid getWindowAndroidChecked() {
        return assertNonNull(getWindowAndroid());
    }

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
     * Returns the content view used for rendered web contents. Can be null if web contents is null.
     */
    @Nullable ContentView getContentView();

    /**
     * Returns the {@link View} displaying the current page in the tab. This can be {@code null}, if
     * the tab is frozen or being initialized or destroyed.
     */
    @Nullable View getView();

    /**
     * Returns the {@link TabViewManager} that is responsible for managing custom {@link View}s
     * shown on top of content in this Tab.
     */
    TabViewManager getTabViewManager();

    /** Returns the id representing this tab. */
    @TabId
    int getId();

    /**
     * Returns parameters that should be used for a lazily initialized or navigated Tab. May be
     * null.
     */
    @Nullable LoadUrlParams getPendingLoadParams();

    /**
     * Returns the URL that is loaded in the current tab. This may not be the same as the last
     * committed URL if a new navigation is in progress.
     */
    GURL getUrl();

    /**
     * Returns the original url of the tab without any Chrome feature modifications applied (e.g.
     * reader mode).
     */
    GURL getOriginalUrl();

    /** Returns the tab title. */
    String getTitle();

    /**
     * Returns the {@link NativePage} associated with the current page, or {@code null} if there is
     * no current page or the current page is displayed using something besides {@link NativePage}.
     */
    @Nullable NativePage getNativePage();

    /** Returns whether or not the {@link Tab} represents a {@link NativePage}. */
    boolean isNativePage();

    /**
     * Returns whether a custom view shown through {@link TabViewManager} is being displayed instead
     * of the current WebContents.
     */
    boolean isShowingCustomView();

    /**
     * Replaces the current NativePage with a empty stand-in for a NativePage. This can be used
     * to reduce memory pressure.
     */
    void freezeNativePage();

    /**
     * Returns the reason the Tab was launched (from a link, external app, etc). May change over
     * time, for instance, to {@code FROM_RESTORE} during tab restoration.
     */
    @TabLaunchType
    int getLaunchType();

    /** Returns the theme color for this tab. */
    @ColorInt
    int getThemeColor();

    /** Returns the background color for the current webpage. */
    @ColorInt
    int getBackgroundColor();

    /**
     * Returns {@code true} if the theme color from contents is valid and can be used for theming.
     */
    boolean isThemingAllowed();

    /**
     * TODO(crbug.com/350654700): clean up usages and remove isIncognito.
     *
     * @return {@code true} if the Tab is in incognito mode.
     * @deprecated Use {@link #isIncognitoBranded()} or {@link #isOffTheRecord()}.
     */
    @Deprecated
    boolean isIncognito();

    /**
     * @see {@link Profile#isOffTheRecord()}.
     * @return {@code true} if the Tab is in an off-the-record profile.
     */
    boolean isOffTheRecord();

    /**
     * @see {@link Profile#isIncognitoBranded()}.
     * @return {@code true} if the Tab is in Incognito branded profile.
     */
    boolean isIncognitoBranded();

    /** Returns whether the {@link Tab} is currently showing an error page. */
    boolean isShowingErrorPage();

    /**
     * Returns true iff the tab doesn't hold a live page. For example, this could happen when the
     * tab holds frozen WebContents state that is yet to be inflated.
     */
    boolean isFrozen();

    /**
     * Returns Whether the tab can currently be interacted with by the user. This requires the view
     * owned by the Tab to be visible and in a state where the user can interact with it (i.e. not
     * in something like the phone tab switcher).
     */
    boolean isUserInteractable();

    /**
     * Returns whether the tab is detached from an activity. This usually means the tab is being
     * reparented; however for headless mode this is always true. A detached tab has a null {@link
     * #getWindowAndroid()} or the window has no activity. Note that this is not the same as the tab
     * being dragged for drag & drop, it also does not imply anything about whether the tab is in a
     * tab model.
     */
    boolean isDetachedFromActivity();

    /** Returns whether this is the activated tab; AKA selected tab, or current tab. */
    boolean isActivated();

    /** Returns whether the tab has a parent collection. */
    boolean hasParentCollection();

    /** Sets Parent for the current Tab and other tab related parent properties. */
    void reparentTab(Tab parent);

    /**
     * Causes this tab to navigate to the specified URL.
     *
     * @param params parameters describing the url load. Note that it is important to set correct
     *     page transition as it is used for ranking URLs in the history so the omnibox can report
     *     suggestions correctly.
     * @return a {@link LoadUrlResult} for this load.
     */
    LoadUrlResult loadUrl(LoadUrlParams params);

    /**
     * Discards the tab by saving its {@link WebContents} to an {@link WebContentsState} and
     * destroying the {@link WebContents}. If the tab is already frozen/discarded this is a no-op.
     * The tab must be closing or inactive to be discarded.
     */
    void discard();

    /**
     * Discards the tabs and stores the URL in the tab's WebContentsState. If the tab is already
     * frozen/discarded this method still appends the navigation entry, but skips the process of
     * discarding the tab. If there is already a pending navigation, it will be replaced by this
     * one.
     *
     * @param params Parameters describing the url load. Note that it is important to set correct
     *     page transition as it is used for ranking URLs in the history so the omnibox can report
     *     suggestions correctly.
     * @param title The title of the tab to use on UI surfaces before it is navigated to.
     */
    void discardAndAppendPendingNavigation(LoadUrlParams params, @Nullable String title);

    /**
     * Loads the tab if it's not loaded (e.g. frozen, lazily loaded, it was background, etc.).
     *
     * @param caller The caller of this method.
     * @return true iff the Tab handled the request.
     */
    boolean loadIfNeeded(int caller);

    /** Reloads the current page content. */
    void reload();

    /**
     * Reloads the current page content.
     *
     * <p>This version ignores the cache and reloads from the network.
     */
    void reloadIgnoringCache();

    /** Stop the current navigation. */
    void stopLoading();

    /** Returns whether the Tab has requested a reload. */
    boolean needsReload();

    /** Returns true iff the tab is loading and an interstitial page is not showing. */
    boolean isLoading();

    /** Returns true iff the tab is performing a restore page load. */
    boolean isBeingRestored();

    /**
     * Returns a value between 0 and 100 reflecting what percentage of the page load is complete.
     */
    float getProgress();

    /** Returns whether or not this tab has a previous navigation entry. */
    boolean canGoBack();

    /** Returns whether or not this tab has a navigation entry after the current one. */
    boolean canGoForward();

    /** Goes to the navigation entry before the current one. */
    void goBack();

    /** Goes to the navigation entry after the current one. */
    void goForward();

    /** Returns true if the {@link Tab} is a custom tab, including CCTs, TWAs and WebAPKs. */
    boolean isCustomTab();

    /** Returns true if the {@link Tab} is in either a TWA or a WebAPK, both types of PWA. */
    boolean isTabInPWA();

    /**
     * Returns true if the {@link Tab} is in the main browser app (i.e. not a CCT, TWA, or WebApk).
     */
    boolean isTabInBrowser();

    /**
     * Returns the last time this tab was shown or the time of its initialization if it wasn't yet
     * shown.
     */
    long getTimestampMillis();

    /**
     * Sets the last time this tab was shown. Used for declutter to mark the tab as "active" after
     * it's restored, but not immediately shown.
     */
    void setTimestampMillis(long timestampMillis);

    /** Returns the parent identifier for the {@link Tab}. */
    @TabId
    int getParentId();

    /**
     * Set the parent identifier for the {@link Tab}. This is equivalent to setting the "opener" tab
     * in desktop Chrome.
     */
    void setParentId(@TabId int parentId);

    /**
     * Returns the root identifier for the {@link Tab}.
     *
     * @deprecated Use {@link #getTabGroupId()} instead. The only exceptions are for tab
     *     persistence, and migrating from root id to tab group id for tab collections.
     */
    @Deprecated
    @TabId
    int getRootId();

    /**
     * Set the root identifier for the {@link Tab}.
     *
     * @param rootId The root identifier to use.
     * @deprecated Use {@link #setTabGroupId()} instead. The only exceptions are declutter, tab
     *     restore, and migrating from root id to tab group id for tab collections.
     */
    @Deprecated
    void setRootId(@TabId int rootId);

    /**
     * Returns the tab group ID of the {@link Tab} or null if not part of a group. Note that during
     * migration from root ID the TabGroupId may be null until tab state is initialized.
     */
    @Nullable Token getTabGroupId();

    /**
     * Sets the tab group ID of the {@link Tab}.
     *
     * @param tabGroupId The {@link Token} to use as the tab group ID or null if not part of a tab
     *     group.
     */
    void setTabGroupId(@Nullable Token tabGroupId);

    /** Returns the user agent type for the {@link Tab} */
    @TabUserAgent
    int getUserAgent();

    /** Set user agent type for the {@link Tab} */
    void setUserAgent(@TabUserAgent int userAgent);

    /** Returns the content state bytes for the {@link Tab} */
    @Nullable WebContentsState getWebContentsState();

    /** Returns the timestamp in milliseconds when the tab was last interacted. */
    long getLastNavigationCommittedTimestampMillis();

    /** Returns launch type at creation. May be {@link TabLaunchType.UNSET} if unknown. */
    @TabLaunchType
    int getTabLaunchTypeAtCreation();

    /** Sets the TabLaunchType for tabs launched with an unset launch type. */
    void setTabLaunchType(@TabLaunchType int launchType);

    /** Update the title for the current page if changed. */
    void updateTitle();

    /**
     * Returns true if the back forward transition is in progress, including web page and native
     * page transitions.
     */
    boolean isDisplayingBackForwardAnimation();

    /** Returns true if we have a WebContents that's navigated to a trusted origin of a TWA. */
    boolean isTrustedWebActivity();

    /** Returns true if the current tab has embedded media experience enabled. */
    boolean shouldEnableEmbeddedMediaExperience();

    /** Returns the content sensitivity of the tab. */
    boolean getTabHasSensitiveContent();

    /**
     * Sets the content sensitivity of the tab.
     *
     * @param contentIsSensitive True if the content is sensitive.
     */
    void setTabHasSensitiveContent(boolean contentIsSensitive);

    /** Returns the current pinned state of the tab. */
    boolean getIsPinned();

    /**
     * Sets the pinned state of the tab.
     *
     * @param isPinned True if the tab is pinned.
     */
    void setIsPinned(boolean isPinned);

    /** Returns the media state of the tab. */
    @MediaState
    int getMediaState();

    /**
     * Sets the media state of the tab.
     *
     * @param mediaState The {@link MediaState} of the tab.
     */
    void setMediaState(@MediaState int mediaState);

    /** Called when the tab is restored from the archived tab model. */
    void onTabRestoredFromArchivedTabModel();

    /** Called when the tab is added to a tab model. */
    void onAddedToTabModel(
            LookAheadObservableSupplier<Tab> currentTabSupplier,
            SelectionStateSupplier selectionStateSupplier);

    /** Called when the tab is removed from a tab model. */
    void onRemovedFromTabModel(
            LookAheadObservableSupplier<Tab> currentTabSupplier, @DetachReason int detachReason);

    /** Returns whether the tab is multi-selected. */
    boolean isMultiSelected();

    /**
     * Returns whether the tab is currently being dragged. To observe this use {@link
     * TabDragStateData}. This exists as a convenience method for plumbing the data to native.
     */
    boolean isDragging();
}
