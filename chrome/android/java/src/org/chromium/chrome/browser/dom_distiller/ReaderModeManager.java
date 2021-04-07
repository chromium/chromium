// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.CommandLine;
import org.chromium.base.IntentUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.UserData;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.IncognitoCustomTabIntentDataProvider;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider.DistillabilityObserver;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.infobar.ReaderModeInfoBar;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Manages UI effects for reader mode including hiding and showing the
 * reader mode and reader mode preferences toolbar icon and hiding the
 * browser controls when a reader mode page has finished loading.
 */
public class ReaderModeManager extends EmptyTabObserver implements UserData {
    /** Possible states that the distiller can be in on a web page. */
    @IntDef({DistillationStatus.POSSIBLE, DistillationStatus.NOT_POSSIBLE,
            DistillationStatus.STARTED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DistillationStatus {
        /** POSSIBLE means reader mode can be entered. */
        int POSSIBLE = 0;

        /** NOT_POSSIBLE means reader mode cannot be entered. */
        int NOT_POSSIBLE = 1;

        /** STARTED means reader mode is currently in reader mode. */
        int STARTED = 2;
    }

    /** The key to access this object from a {@Tab}. */
    public static final Class<ReaderModeManager> USER_DATA_KEY = ReaderModeManager.class;

    /** The scheme used to access DOM-Distiller. */
    public static final String DOM_DISTILLER_SCHEME = "chrome-distiller";

    /** The intent extra that indicates origin from Reader Mode */
    public static final String EXTRA_READER_MODE_PARENT =
            "org.chromium.chrome.browser.dom_distiller.EXTRA_READER_MODE_PARENT";

    /** The url of the last page visited if the last page was reader mode page.  Otherwise null. */
    private GURL mReaderModePageUrl;

    /** Whether the fact that the current web page was distillable or not has been recorded. */
    private boolean mIsUmaRecorded;

    /** The WebContentsObserver responsible for updates to the distillation status of the tab. */
    private WebContentsObserver mWebContentsObserver;

    /** The distillation status of the tab. */
    @DistillationStatus
    private int mDistillationStatus;

    /** If the infobar was closed due to the close button. */
    private boolean mIsDismissed;

    /**
     * The URL that distiller is using for this tab. This is used to check if a result comes back
     * from distiller and the user has already loaded a new URL.
     */
    private GURL mDistillerUrl;

    /** Used to flag the the infobar was shown and recorded by UMA. */
    private boolean mShowInfoBarRecorded;

    /** Whether or not the current tab is a Reader Mode page. */
    private boolean mIsViewingReaderModePage;

    /** The time that the user started viewing Reader Mode content. */
    private long mViewStartTimeMs;

    /** The distillability observer attached to the tab. */
    private DistillabilityObserver mDistillabilityObserver;

    /** Whether this manager and tab have been destroyed. */
    private boolean mIsDestroyed;

    /** The tab this manager is attached to. */
    private Tab mTab;

    // Hold on to the InterceptNavigationDelegate that the custom tab uses.
    InterceptNavigationDelegate mCustomTabNavigationDelegate;

    ReaderModeManager(Tab tab) {
        super();
        mTab = tab;
        mTab.addObserver(this);
    }

    /**
     * Create an instance of the {@link ReaderModeManager} for the provided tab.
     * @param tab The tab that will have a manager instance attached to it.
     */
    public static void createForTab(Tab tab) {
        tab.getUserDataHost().setUserData(USER_DATA_KEY, new ReaderModeManager(tab));
    }

    /**
     * Clear the status map and references to other objects.
     */
    @Override
    public void destroy() {
        if (mWebContentsObserver != null) mWebContentsObserver.destroy();
        mIsDestroyed = true;
    }

    @Override
    public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
        // If a distiller URL was loaded and this is a custom tab, add a navigation
        // handler to bring any navigations back to the main chrome activity.
        Activity activity = TabUtils.getActivity(tab);
        int uiType = CustomTabsUiType.DEFAULT;
        if (activity != null && activity.getIntent().getExtras() != null) {
            uiType = activity.getIntent().getExtras().getInt(
                    CustomTabIntentDataProvider.EXTRA_UI_TYPE);
        }
        if (tab == null || uiType != CustomTabsUiType.READER_MODE
                || !DomDistillerUrlUtils.isDistilledPage(params.getUrl())) {
            return;
        }

        WebContents webContents = tab.getWebContents();
        if (webContents == null) return;

        mCustomTabNavigationDelegate = (navParams) -> {
            if (DomDistillerUrlUtils.isDistilledPage(navParams.url)
                    || navParams.isExternalProtocol) {
                return false;
            }

            Intent returnIntent =
                    new Intent(Intent.ACTION_VIEW, Uri.parse(navParams.url.getSpec()));
            returnIntent.setClassName(activity, ChromeLauncherActivity.class.getName());

            // Set the parent ID of the tab to be created.
            returnIntent.putExtra(EXTRA_READER_MODE_PARENT,
                    IntentUtils.safeGetInt(activity.getIntent().getExtras(),
                            EXTRA_READER_MODE_PARENT, Tab.INVALID_TAB_ID));

            activity.startActivity(returnIntent);
            activity.finish();
            return true;
        };

        DomDistillerTabUtils.setInterceptNavigationDelegate(
                mCustomTabNavigationDelegate, webContents);
    }

    @Override
    public void onShown(Tab shownTab, @TabSelectionType int type) {
        // If the reader infobar was dismissed, stop here.
        if (mIsDismissed) return;

        mDistillationStatus = DistillationStatus.NOT_POSSIBLE;
        mDistillerUrl = shownTab.getUrl();

        if (mDistillabilityObserver == null) setDistillabilityObserver(shownTab);

        if (DomDistillerUrlUtils.isDistilledPage(shownTab.getUrl()) && !mIsViewingReaderModePage) {
            onStartedReaderMode();
        }

        // Make sure there is a WebContentsObserver on this tab's WebContents.
        if (mWebContentsObserver == null && mTab.getWebContents() != null) {
            mWebContentsObserver = createWebContentsObserver();
        }
        tryShowingInfoBar();
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int reason) {
        if (mIsViewingReaderModePage) {
            long timeMs = onExitReaderMode();
            recordReaderModeViewDuration(timeMs);
        }
    }

    @Override
    public void onDestroyed(Tab tab) {
        if (tab == null) return;

        // If the infobar was not shown for the previous navigation, record it now.
        if (!mShowInfoBarRecorded) {
            recordInfoBarVisibilityForNavigation(false);
        }
        if (mIsViewingReaderModePage) {
            long timeMs = onExitReaderMode();
            recordReaderModeViewDuration(timeMs);
        }
        TabDistillabilityProvider.get(tab).removeObserver(mDistillabilityObserver);

        removeTabState();
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        // Intentionally do nothing to prevent automatic observer removal on detachment.
    }

    /** Clear the reader mode state for this manager. */
    private void removeTabState() {
        if (mWebContentsObserver != null) mWebContentsObserver.destroy();
        mDistillationStatus = DistillationStatus.POSSIBLE;
        mIsDismissed = false;
        mDistillerUrl = null;
        mShowInfoBarRecorded = false;
        mIsViewingReaderModePage = false;
        mDistillabilityObserver = null;
    }

    @Override
    public void onContentChanged(Tab tab) {
        // If the content change was because of distiller switching web contents or Reader Mode has
        // already been dismissed for this tab do nothing.
        if (mIsDismissed && !DomDistillerUrlUtils.isDistilledPage(tab.getUrl())) return;

        // If the tab state already existed, only reset the relevant data. Things like view duration
        // need to be preserved.
        mDistillationStatus = DistillationStatus.NOT_POSSIBLE;
        mDistillerUrl = tab.getUrl();

        if (tab.getWebContents() != null) {
            mWebContentsObserver = createWebContentsObserver();
            if (DomDistillerUrlUtils.isDistilledPage(tab.getUrl())) {
                mDistillationStatus = DistillationStatus.STARTED;
                mReaderModePageUrl = tab.getUrl();
            }
        }
    }

    /** A notification that the user started viewing Reader Mode. */
    private void onStartedReaderMode() {
        mIsViewingReaderModePage = true;
        mViewStartTimeMs = SystemClock.elapsedRealtime();
    }

    /**
     * A notification that the user is no longer viewing Reader Mode. This could be because of a
     * navigation away from the page, switching tabs, or closing the browser.
     * @return The amount of time in ms that the user spent viewing Reader Mode.
     */
    private long onExitReaderMode() {
        mIsViewingReaderModePage = false;
        return SystemClock.elapsedRealtime() - mViewStartTimeMs;
    }

    /**
     * Record if the infobar became visible on the current page. This can be overridden for testing.
     * @param visible If the infobar was visible at any time.
     */
    private void recordInfoBarVisibilityForNavigation(boolean visible) {
        RecordHistogram.recordBooleanHistogram("DomDistiller.ReaderShownForPageLoad", visible);
    }

    /** A notification that the infobar was closed without being used. */
    public void onClosed() {
        RecordHistogram.recordBooleanHistogram("DomDistiller.InfoBarUsage", false);
        mIsDismissed = true;
    }

    private WebContentsObserver createWebContentsObserver() {
        return new WebContentsObserver(mTab.getWebContents()) {
            /** Whether or not the previous navigation should be removed. */
            private boolean mShouldRemovePreviousNavigation;

            /** The index of the last committed distiller page in history. */
            private int mLastDistillerPageIndex;

            @Override
            public void didStartNavigation(NavigationHandle navigation) {
                if (!navigation.isInMainFrame() || navigation.isSameDocument()) return;

                // Reader Mode should not pollute the navigation stack. To avoid this, watch for
                // navigations and prepare to remove any that are "chrome-distiller" urls.
                NavigationController controller = mWebContents.get().getNavigationController();
                int index = controller.getLastCommittedEntryIndex();
                NavigationEntry entry = controller.getEntryAtIndex(index);

                if (entry != null && DomDistillerUrlUtils.isDistilledPage(entry.getUrl())) {
                    mShouldRemovePreviousNavigation = true;
                    mLastDistillerPageIndex = index;
                }

                if (mIsDestroyed) return;

                mDistillerUrl = navigation.getUrl();
                if (DomDistillerUrlUtils.isDistilledPage(navigation.getUrl())) {
                    mDistillationStatus = DistillationStatus.STARTED;
                    mReaderModePageUrl = navigation.getUrl();
                }
            }

            @Override
            public void didFinishNavigation(NavigationHandle navigation) {
                // TODO(cjhopman): This should possibly ignore navigations that replace the entry
                // (like those from history.replaceState()).
                if (!navigation.hasCommitted() || !navigation.isInMainFrame()
                        || navigation.isSameDocument()) {
                    return;
                }

                if (mShouldRemovePreviousNavigation) {
                    mShouldRemovePreviousNavigation = false;
                    NavigationController controller = mWebContents.get().getNavigationController();
                    if (controller.getEntryAtIndex(mLastDistillerPageIndex) != null) {
                        controller.removeEntryAtIndex(mLastDistillerPageIndex);
                    }
                }

                if (mIsDestroyed) return;

                mDistillationStatus = DistillationStatus.POSSIBLE;
                if (mReaderModePageUrl == null
                        || !navigation.getUrl().equals(
                                DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(
                                        mReaderModePageUrl))) {
                    mDistillationStatus = DistillationStatus.NOT_POSSIBLE;
                    mIsUmaRecorded = false;
                }
                mReaderModePageUrl = null;

                if (mDistillationStatus == DistillationStatus.POSSIBLE) tryShowingInfoBar();
            }

            @Override
            public void navigationEntryCommitted(LoadCommittedDetails details) {
                if (mIsDestroyed) return;
                // Reset closed state of reader mode in this tab once we know a navigation is
                // happening.
                mIsDismissed = false;

                // If the infobar was not shown for the previous navigation, record it now.
                if (mTab != null && !mTab.isNativePage() && !mTab.isBeingRestored()) {
                    recordInfoBarVisibilityForNavigation(false);
                }
                mShowInfoBarRecorded = false;

                if (mTab != null && !DomDistillerUrlUtils.isDistilledPage(mTab.getUrl())
                        && mIsViewingReaderModePage) {
                    long timeMs = onExitReaderMode();
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

    /** Try showing the reader mode infobar. */
    @VisibleForTesting
    void tryShowingInfoBar() {
        if (mTab == null || mTab.getWebContents() == null) return;

        // Test if the user is requesting the desktop site. Ignore this if distiller is set to
        // ALWAYS_TRUE.
        boolean usingRequestDesktopSite =
                mTab.getWebContents().getNavigationController().getUseDesktopUserAgent()
                && !DomDistillerTabUtils.isHeuristicAlwaysTrue();

        if (usingRequestDesktopSite || mDistillationStatus != DistillationStatus.POSSIBLE
                || mIsDismissed) {
            return;
        }

        ReaderModeInfoBar.showReaderModeInfoBar(mTab);
    }

    public void activateReaderMode() {
        RecordHistogram.recordBooleanHistogram("DomDistiller.InfoBarUsage", true);

        if (DomDistillerTabUtils.isCctMode() && !SysUtils.isLowEndDevice()) {
            distillInCustomTab();
        } else {
            navigateToReaderMode();
        }
    }

    /** Navigate the current tab to a Reader Mode URL. */
    private void navigateToReaderMode() {
        WebContents webContents = mTab.getWebContents();
        if (webContents == null) return;

        GURL url = webContents.getLastCommittedUrl();

        onStartedReaderMode();

        // Make sure to exit fullscreen mode before navigating.
        getFullscreenManager().onExitFullscreen(mTab);

        // RenderWidgetHostViewAndroid hides the controls after transitioning to reader mode.
        // See the long history of the issue in https://crbug.com/825765, https://crbug.com/853686,
        // https://crbug.com/861618, https://crbug.com/922388.
        // TODO(pshmakov): find a proper solution instead of this workaround.
        getBrowserControlsVisibilityManager()
                .getBrowserVisibilityDelegate()
                .showControlsTransient();

        DomDistillerTabUtils.distillCurrentPageAndView(webContents);
    }

    private BrowserControlsVisibilityManager getBrowserControlsVisibilityManager() {
        // TODO(1069815): Remove this ChromeActivity cast once BrowserControlsManager is
        //                accessible via another mechanism.
        ChromeActivity activity = (ChromeActivity) TabUtils.getActivity(mTab);
        return activity.getBrowserControlsManager();
    }

    private FullscreenManager getFullscreenManager() {
        // TODO(1069815): Remove this ChromeActivity cast once FullscreenManager is
        //                accessible via another mechanism.
        ChromeActivity activity = (ChromeActivity) TabUtils.getActivity(mTab);
        return activity.getFullscreenManager();
    }

    private void distillInCustomTab() {
        Activity activity = TabUtils.getActivity(mTab);
        WebContents webContents = mTab.getWebContents();
        if (webContents == null) return;

        GURL url = webContents.getLastCommittedUrl();

        onStartedReaderMode();

        DomDistillerTabUtils.distillCurrentPage(webContents);

        String distillerUrl = DomDistillerUrlUtils.getDistillerViewUrlFromUrl(
                DOM_DISTILLER_SCHEME, url.getSpec(), webContents.getTitle());

        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setShowTitle(true);
        builder.setColorScheme(ColorUtils.inNightMode(activity)
                        ? CustomTabsIntent.COLOR_SCHEME_DARK
                        : CustomTabsIntent.COLOR_SCHEME_LIGHT);
        CustomTabsIntent customTabsIntent = builder.build();
        customTabsIntent.intent.setClassName(activity, CustomTabActivity.class.getName());

        // Customize items on menu as Reader Mode UI to show 'Find in page' and 'Preference' only.
        CustomTabIntentDataProvider.addReaderModeUIExtras(customTabsIntent.intent);

        // Add the parent ID as an intent extra for back button functionality.
        customTabsIntent.intent.putExtra(EXTRA_READER_MODE_PARENT, mTab.getId());

        // Use Incognito CCT if the source page is in Incognito mode. This is gated by
        // flag ChromeFeatureList.CCT_INCOGNITO.
        if (mTab.isIncognito()) {
            IncognitoCustomTabIntentDataProvider.addIncongitoExtrasForChromeFeatures(
                    customTabsIntent.intent, IntentHandler.IncognitoCCTCallerId.READER_MODE);
        }

        customTabsIntent.launchUrl(activity, Uri.parse(distillerUrl));
    }

    /**
     * Set the observer for updating reader mode status based on whether or not the page should
     * be viewed in reader mode.
     * @param tabToObserve The tab to attach the observer to.
     */
    private void setDistillabilityObserver(final Tab tabToObserve) {
        mDistillabilityObserver = (tab, isDistillable, isLast, isMobileOptimized) -> {
            // Make sure the page didn't navigate while waiting for a response.
            if (!tab.getUrl().equals(mDistillerUrl)) return;

            if (isDistillable
                    && !(isMobileOptimized
                            && DomDistillerTabUtils.shouldExcludeMobileFriendly(tabToObserve))) {
                mDistillationStatus = DistillationStatus.POSSIBLE;
                tryShowingInfoBar();
            } else {
                mDistillationStatus = DistillationStatus.NOT_POSSIBLE;
            }
            if (!mIsUmaRecorded && (mDistillationStatus == DistillationStatus.POSSIBLE || isLast)) {
                mIsUmaRecorded = true;
                RecordHistogram.recordBooleanHistogram("DomDistiller.PageDistillable",
                        mDistillationStatus == DistillationStatus.POSSIBLE);
            }
        };
        TabDistillabilityProvider.get(tabToObserve).addObserver(mDistillabilityObserver);
    }

    @VisibleForTesting
    int getDistillationStatus() {
        return mDistillationStatus;
    }

    /** @return Whether Reader mode and its new UI are enabled. */
    public static boolean isEnabled() {
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
