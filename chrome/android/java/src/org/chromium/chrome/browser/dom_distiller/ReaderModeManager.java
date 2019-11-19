// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.CommandLine;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.infobar.ReaderModeInfoBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.util.HashMap;
import java.util.Map;

/**
 * Manages UI effects for reader mode including hiding and showing the
 * reader mode and reader mode preferences toolbar icon and hiding the
 * browser controls when a reader mode page has finished loading.
 */
public class ReaderModeManager extends TabModelSelectorTabObserver {
    /** POSSIBLE means reader mode can be entered. */
    public static final int POSSIBLE = 0;

    /** NOT_POSSIBLE means reader mode cannot be entered. */
    public static final int NOT_POSSIBLE = 1;

    /** STARTED means reader mode is currently in reader mode. */
    public static final int STARTED = 2;

    /** The scheme used to access DOM-Distiller. */
    public static final String DOM_DISTILLER_SCHEME = "chrome-distiller";

    /** The intent extra that indicates origin from Reader Mode */
    public static final String EXTRA_READER_MODE_PARENT =
            "org.chromium.chrome.browser.dom_distiller.EXTRA_READER_MODE_PARENT";

    /** The url of the last page visited if the last page was reader mode page.  Otherwise null. */
    private String mReaderModePageUrl;

    /** Whether the fact that the current web page was distillable or not has been recorded. */
    private boolean mIsUmaRecorded;

    /** The per-tab state of distillation. */
    protected Map<Integer, ReaderModeTabInfo> mTabStatusMap;

    /** The ChromeActivity that this infobar exists in. */
    private ChromeActivity mChromeActivity;

    /** The primary means of getting the currently active tab. */
    private TabModelSelector mTabModelSelector;

    // Hold on to the InterceptNavigationDelegate that the custom tab uses.
    InterceptNavigationDelegate mCustomTabNavigationDelegate;

    private TabModelSelectorTabModelObserver mTabModelSelectorObserver;

    public ReaderModeManager(TabModelSelector selector, ChromeActivity activity) {
        super(selector);
        mTabModelSelector = selector;
        mChromeActivity = activity;
        mTabStatusMap = new HashMap<>();
    }

    /**
     * Clear the status map and references to other objects.
     */
    @Override
    public void destroy() {
        super.destroy();
        for (Map.Entry<Integer, ReaderModeTabInfo> e : mTabStatusMap.entrySet()) {
            Tab tab = mTabModelSelector.getTabById(e.getKey());
            ReaderModeTabInfo info = e.getValue();
            if (info.webContentsObserver != null) info.webContentsObserver.destroy();
            if (tab != null) {
                TabDistillabilityProvider.get(tab).removeObserver(info.distillabilityObserver);
            }
        }
        mTabStatusMap.clear();

        DomDistillerUIUtils.destroy(this);

        mChromeActivity = null;
        mTabModelSelector = null;
    }

    // TabModelSelectorTabObserver:

    @Override
    public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
        // If a distiller URL was loaded and this is a custom tab, add a navigation
        // handler to bring any navigations back to the main chrome activity.
        if (tab == null || !mChromeActivity.isCustomTab()
                || !DomDistillerUrlUtils.isDistilledPage(params.getUrl())) {
            return;
        }

        WebContents webContents = tab.getWebContents();
        if (webContents == null) return;

        mCustomTabNavigationDelegate = new InterceptNavigationDelegate() {
            @Override
            public boolean shouldIgnoreNavigation(NavigationParams params) {
                if (DomDistillerUrlUtils.isDistilledPage(params.url) || params.isExternalProtocol) {
                    return false;
                }

                Intent returnIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(params.url));
                returnIntent.setClassName(mChromeActivity, ChromeLauncherActivity.class.getName());

                // Set the parent ID of the tab to be created.
                returnIntent.putExtra(EXTRA_READER_MODE_PARENT,
                        IntentUtils.safeGetInt(mChromeActivity.getIntent().getExtras(),
                                EXTRA_READER_MODE_PARENT, Tab.INVALID_TAB_ID));

                mChromeActivity.startActivity(returnIntent);
                mChromeActivity.finish();
                return true;
            }
        };

        DomDistillerTabUtils.setInterceptNavigationDelegate(
                mCustomTabNavigationDelegate, webContents);
    }

    @Override
    public void onShown(Tab shownTab, @TabSelectionType int type) {
        if (mTabModelSelector == null) return;

        int shownTabId = shownTab.getId();

        // If the reader infobar was dismissed, stop here.
        if (mTabStatusMap.containsKey(shownTabId) && mTabStatusMap.get(shownTabId).isDismissed) {
            return;
        }

        // Set this manager as the active one for the UI utils.
        DomDistillerUIUtils.setReaderModeManagerDelegate(this);

        // If there is no state info for this tab, create it.
        ReaderModeTabInfo tabInfo = mTabStatusMap.get(shownTabId);
        if (tabInfo == null) {
            tabInfo = new ReaderModeTabInfo();
            tabInfo.status = NOT_POSSIBLE;
            tabInfo.url = shownTab.getUrl();
            mTabStatusMap.put(shownTabId, tabInfo);
        }

        if (tabInfo.distillabilityObserver == null) setDistillabilityObserver(shownTab);

        if (DomDistillerUrlUtils.isDistilledPage(shownTab.getUrl())
                && !tabInfo.isViewingReaderModePage) {
            tabInfo.onStartedReaderMode();
        }

        // Make sure there is a WebContentsObserver on this tab's WebContents.
        if (tabInfo.webContentsObserver == null) {
            tabInfo.webContentsObserver = createWebContentsObserver(shownTab.getWebContents());
        }

        tryShowingInfoBar();
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int reason) {
        ReaderModeTabInfo info = mTabStatusMap.get(tab.getId());
        if (info != null && info.isViewingReaderModePage) {
            long timeMs = info.onExitReaderMode();
            recordReaderModeViewDuration(timeMs);
        }
    }

    @Override
    public void onDestroyed(Tab tab) {
        if (tab == null) return;

        // If the infobar was not shown for the previous navigation, record it now.
        ReaderModeTabInfo info = mTabStatusMap.get(tab.getId());
        if (info != null) {
            if (!info.showInfoBarRecorded) {
                recordInfoBarVisibilityForNavigation(false);
            }
            if (info.isViewingReaderModePage) {
                long timeMs = info.onExitReaderMode();
                recordReaderModeViewDuration(timeMs);
            }
            TabDistillabilityProvider.get(tab).removeObserver(info.distillabilityObserver);
        }

        removeTabState(tab.getId());
    }

    /**
     * Clean up the state associated with a tab.
     * @param tabId The target tab ID.
     */
    private void removeTabState(int tabId) {
        if (!mTabStatusMap.containsKey(tabId)) return;
        ReaderModeTabInfo tabInfo = mTabStatusMap.get(tabId);
        if (tabInfo.webContentsObserver != null) {
            tabInfo.webContentsObserver.destroy();
        }
        mTabStatusMap.remove(tabId);
    }

    @Override
    public void onContentChanged(Tab tab) {
        // If the content change was because of distiller switching web contents or Reader Mode has
        // already been dismissed for this tab do nothing.
        if (mTabStatusMap.containsKey(tab.getId()) && mTabStatusMap.get(tab.getId()).isDismissed
                && !DomDistillerUrlUtils.isDistilledPage(tab.getUrl())) {
            return;
        }

        ReaderModeTabInfo tabInfo = mTabStatusMap.get(tab.getId());
        if (!mTabStatusMap.containsKey(tab.getId())) {
            tabInfo = new ReaderModeTabInfo();
            mTabStatusMap.put(tab.getId(), tabInfo);
        }
        // If the tab state already existed, only reset the relevant data. Things like view duration
        // need to be preserved.
        tabInfo.status = NOT_POSSIBLE;
        tabInfo.url = tab.getUrl();

        if (tab.getWebContents() != null) {
            tabInfo.webContentsObserver = createWebContentsObserver(tab.getWebContents());
            if (DomDistillerUrlUtils.isDistilledPage(tab.getUrl())) {
                tabInfo.status = STARTED;
                mReaderModePageUrl = tab.getUrl();
            }
        }
    }

    /**
     * Record if the infobar became visible on the current page. This can be overridden for testing.
     * @param visible If the infobar was visible at any time.
     */
    protected void recordInfoBarVisibilityForNavigation(boolean visible) {
        RecordHistogram.recordBooleanHistogram("DomDistiller.ReaderShownForPageLoad", visible);
    }

    /**
     * Notify the manager that the panel has completely closed.
     */
    public void onClosed(@StateChangeReason int reason) {
        if (mTabModelSelector == null) return;

        RecordHistogram.recordBooleanHistogram("DomDistiller.InfoBarUsage", false);

        int currentTabId = mTabModelSelector.getCurrentTabId();
        if (!mTabStatusMap.containsKey(currentTabId)) return;
        mTabStatusMap.get(currentTabId).isDismissed = true;
    }

    /**
     * Get the WebContents of the page that is being distilled.
     * @return The WebContents for the currently visible tab.
     */
    public WebContents getBasePageWebContents() {
        Tab tab = mTabModelSelector.getCurrentTab();
        if (tab == null) return null;

        return tab.getWebContents();
    }

    /**
     * @return True if the keyboard might be showing. This is not 100% accurate; see
     *         {@link KeyboardVisibilityDelegate#isKeyboardShowing}.
     */
    protected boolean isKeyboardShowing() {
        return mChromeActivity != null && mChromeActivity.getWindowAndroid() != null
                && mChromeActivity.getWindowAndroid().getKeyboardDelegate().isKeyboardShowing(
                           mChromeActivity, mChromeActivity.findViewById(android.R.id.content));
    }

    protected WebContentsObserver createWebContentsObserver(final WebContents webContents) {
        final int readerTabId = mTabModelSelector.getCurrentTabId();
        if (readerTabId == Tab.INVALID_TAB_ID) return null;

        return new WebContentsObserver(webContents) {
            /** Whether or not the previous navigation should be removed. */
            private boolean mShouldRemovePreviousNavigation;

            /** The index of the last committed distiller page in history. */
            private int mLastDistillerPageIndex;

            @Override
            public void didStartNavigation(NavigationHandle navigation) {
                if (!navigation.isInMainFrame() || navigation.isSameDocument()) return;

                // Reader Mode should not pollute the navigation stack. To avoid this, watch for
                // navigations and prepare to remove any that are "chrome-distiller" urls.
                NavigationController controller = webContents.getNavigationController();
                int index = controller.getLastCommittedEntryIndex();
                NavigationEntry entry = controller.getEntryAtIndex(index);

                if (entry != null && DomDistillerUrlUtils.isDistilledPage(entry.getUrl())) {
                    mShouldRemovePreviousNavigation = true;
                    mLastDistillerPageIndex = index;
                }

                // Make sure the tab was not destroyed.
                ReaderModeTabInfo tabInfo = mTabStatusMap.get(readerTabId);
                if (tabInfo == null) return;

                tabInfo.url = navigation.getUrl();
                if (DomDistillerUrlUtils.isDistilledPage(navigation.getUrl())) {
                    tabInfo.status = STARTED;
                    mReaderModePageUrl = navigation.getUrl();
                }
            }

            @Override
            public void didFinishNavigation(NavigationHandle navigation) {
                // TODO(cjhopman): This should possibly ignore navigations that replace the entry
                // (like those from history.replaceState()).
                if (!navigation.hasCommitted() || !navigation.isInMainFrame()
                        || navigation.isSameDocument())
                    return;

                if (mShouldRemovePreviousNavigation) {
                    mShouldRemovePreviousNavigation = false;
                    NavigationController controller = webContents.getNavigationController();
                    if (controller.getEntryAtIndex(mLastDistillerPageIndex) != null) {
                        controller.removeEntryAtIndex(mLastDistillerPageIndex);
                    }
                }

                // Make sure the tab was not destroyed.
                ReaderModeTabInfo tabInfo = mTabStatusMap.get(readerTabId);
                if (tabInfo == null) return;

                tabInfo.status = POSSIBLE;
                if (!TextUtils.equals(navigation.getUrl(),
                            DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(
                                    mReaderModePageUrl))) {
                    tabInfo.status = NOT_POSSIBLE;
                    mIsUmaRecorded = false;
                }
                mReaderModePageUrl = null;

                if (tabInfo.status == POSSIBLE) tryShowingInfoBar();
            }

            @Override
            public void navigationEntryCommitted() {
                // Make sure the tab was not destroyed.
                ReaderModeTabInfo tabInfo = mTabStatusMap.get(readerTabId);
                if (tabInfo == null) return;
                // Reset closed state of reader mode in this tab once we know a navigation is
                // happening.
                tabInfo.isDismissed = false;

                // If the infobar was not shown for the previous navigation, record it now.
                Tab curTab = mTabModelSelector.getTabById(readerTabId);
                if (curTab != null && !curTab.isNativePage() && !curTab.isBeingRestored()) {
                    recordInfoBarVisibilityForNavigation(false);
                }
                tabInfo.showInfoBarRecorded = false;

                if (curTab != null && !DomDistillerUrlUtils.isDistilledPage(curTab.getUrl())
                        && tabInfo.isViewingReaderModePage) {
                    long timeMs = tabInfo.onExitReaderMode();
                    recordReaderModeViewDuration(timeMs);
                }
            }
        };
    }

    /**
     * Record the amount of time the user spent in Reader Mode.
     * @param timeMs The amount of time in ms that the user spent in Reader Mode.
     */
    private void recordReaderModeViewDuration(long timeMs) {
        RecordHistogram.recordLongTimesHistogram("DomDistiller.Time.ViewingReaderModePage", timeMs);
    }

    /**
     * Try showing the reader mode infobar.
     */
    protected void tryShowingInfoBar() {
        if (mTabModelSelector == null) return;

        int currentTabId = mTabModelSelector.getCurrentTabId();
        if (currentTabId == Tab.INVALID_TAB_ID) return;

        // Test if the user is requesting the desktop site. Ignore this if distiller is set to
        // ALWAYS_TRUE.
        boolean usingRequestDesktopSite = getBasePageWebContents() != null
                && getBasePageWebContents().getNavigationController().getUseDesktopUserAgent()
                && !DomDistillerTabUtils.isHeuristicAlwaysTrue();

        if (!mTabStatusMap.containsKey(currentTabId) || usingRequestDesktopSite
                || mTabStatusMap.get(currentTabId).status != POSSIBLE
                || mTabStatusMap.get(currentTabId).isDismissed) {
            return;
        }

        ReaderModeInfoBar.showReaderModeInfoBar(mTabModelSelector.getCurrentTab());
    }

    public void activateReaderMode() {
        RecordHistogram.recordBooleanHistogram("DomDistiller.InfoBarUsage", true);

        if (DomDistillerTabUtils.isCctMode() && !SysUtils.isLowEndDevice()) {
            distillInCustomTab();
        } else {
            navigateToReaderMode();
        }
    }

    /**
     * Navigate the current tab to a Reader Mode URL.
     */
    private void navigateToReaderMode() {
        WebContents baseWebContents = getBasePageWebContents();
        if (baseWebContents == null || mChromeActivity == null || mTabModelSelector == null) return;

        String url = baseWebContents.getLastCommittedUrl();
        if (url == null) return;

        ReaderModeTabInfo info = mTabStatusMap.get(mTabModelSelector.getCurrentTabId());
        if (info != null) info.onStartedReaderMode();

        // Make sure to exit fullscreen mode before navigating.
        Tab currentTab = mTabModelSelector.getCurrentTab();
        currentTab.exitFullscreenMode();

        // RenderWidgetHostViewAndroid hides the controls after transitioning to reader mode.
        // See the long history of the issue in https://crbug.com/825765, https://crbug.com/853686,
        // https://crbug.com/861618, https://crbug.com/922388.
        // TODO(pshmakov): find a proper solution instead of this workaround.
        showControlsTransient(currentTab);

        DomDistillerTabUtils.distillCurrentPageAndView(getBasePageWebContents());
    }

    private void showControlsTransient(Tab tab) {
        assert mChromeActivity != null;
        FullscreenManager fullscreenManager = mChromeActivity.getFullscreenManager();
        if (!(fullscreenManager instanceof ChromeFullscreenManager)) return;
        ((ChromeFullscreenManager) fullscreenManager).getBrowserVisibilityDelegate()
                .showControlsTransient();
    }

    private void distillInCustomTab() {
        WebContents baseWebContents = getBasePageWebContents();
        if (baseWebContents == null || mChromeActivity == null || mTabModelSelector == null) return;

        String url = baseWebContents.getLastCommittedUrl();
        if (url == null) return;

        ReaderModeTabInfo info = mTabStatusMap.get(mTabModelSelector.getCurrentTabId());
        if (info != null) info.onStartedReaderMode();

        DomDistillerTabUtils.distillCurrentPage(baseWebContents);

        String distillerUrl =
                DomDistillerUrlUtils.getDistillerViewUrlFromUrl(DOM_DISTILLER_SCHEME, url);

        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setShowTitle(true);
        builder.setColorScheme(mChromeActivity.getNightModeStateProvider().isInNightMode()
                        ? CustomTabsIntent.COLOR_SCHEME_DARK
                        : CustomTabsIntent.COLOR_SCHEME_LIGHT);
        CustomTabsIntent customTabsIntent = builder.build();
        customTabsIntent.intent.setClassName(mChromeActivity, CustomTabActivity.class.getName());

        // Customize items on menu as Reader Mode UI to show 'Find in page' and 'Preference' only.
        CustomTabIntentDataProvider.addReaderModeUIExtras(customTabsIntent.intent);

        // Add the parent ID as an intent extra for back button functionality.
        customTabsIntent.intent.putExtra(
                EXTRA_READER_MODE_PARENT, mTabModelSelector.getCurrentTabId());

        customTabsIntent.launchUrl(mChromeActivity, Uri.parse(distillerUrl));
    }

    /**
     * Set the observer for updating reader mode status based on whether or not the page should
     * be viewed in reader mode.
     * @param tabToObserve The tab to attach the observer to.
     */
    private void setDistillabilityObserver(final Tab tabToObserve) {
        mTabStatusMap.get(tabToObserve.getId()).distillabilityObserver =
                (tab, isDistillable, isLast, isMobileOptimized) -> {
            if (mTabModelSelector == null) return;

            ReaderModeTabInfo tabInfo = mTabStatusMap.get(tab.getId());

            // It is possible that the tab was destroyed before this callback happens.
            // TODO(wychen/mdjones): Remove the callback when a Tab/WebContents is
            // destroyed so that this never happens.
            if (tabInfo == null) return;

            // Make sure the page didn't navigate while waiting for a response.
            if (!tab.getUrl().equals(tabInfo.url)) return;

            boolean excludedMobileFriendly =
                    DomDistillerTabUtils.shouldExcludeMobileFriendly() && isMobileOptimized;
            if (isDistillable && !excludedMobileFriendly) {
                tabInfo.status = POSSIBLE;
                // The user may have changed tabs.
                if (tab.getId() == mTabModelSelector.getCurrentTabId()) {
                    tryShowingInfoBar();
                }
            } else {
                tabInfo.status = NOT_POSSIBLE;
            }
            if (!mIsUmaRecorded && (tabInfo.status == POSSIBLE || isLast)) {
                mIsUmaRecorded = true;
                RecordHistogram.recordBooleanHistogram(
                        "DomDistiller.PageDistillable", tabInfo.status == POSSIBLE);
            }
        };
        TabDistillabilityProvider.get(tabToObserve)
                .addObserver(mTabStatusMap.get(tabToObserve.getId()).distillabilityObserver);
    }

    /**
     * @return Whether Reader mode and its new UI are enabled.
     * @param context A context
     */
    public static boolean isEnabled(Context context) {
        if (context == null) return false;

        boolean enabled = CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_DOM_DISTILLER)
                && !CommandLine.getInstance().hasSwitch(
                           ChromeSwitches.DISABLE_READER_MODE_BOTTOM_BAR)
                && DomDistillerTabUtils.isDistillerHeuristicsEnabled();
        return enabled;
    }

    /**
     * Determine if Reader Mode created the intent for a tab being created.
     * @param intent The Intent creating a new tab.
     * @return True whether the intent was created by Reader Mode.
     */
    public static boolean isReaderModeCreatedIntent(@NonNull Intent intent) {
        int readerParentId = IntentUtils.safeGetInt(
                intent.getExtras(), ReaderModeManager.EXTRA_READER_MODE_PARENT, Tab.INVALID_TAB_ID);
        return readerParentId != Tab.INVALID_TAB_ID;
    }
}
