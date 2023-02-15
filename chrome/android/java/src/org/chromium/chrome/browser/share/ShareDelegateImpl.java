// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.history_clusters.HistoryClustersTabHelper;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.printing.TabPrinter;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextHelper;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetPropertyModelBuilder;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.ui_metrics.CanonicalURLResult;
import org.chromium.content_public.browser.WebContents;
import org.chromium.printing.PrintManagerDelegateImpl;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Implementation of share interface. Mostly a wrapper around ShareSheetCoordinator.
 */
public class ShareDelegateImpl implements ShareDelegate {
    static final String CANONICAL_URL_RESULT_HISTOGRAM = "Mobile.CanonicalURLResult";

    private final BottomSheetController mBottomSheetController;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final Supplier<Tab> mTabProvider;
    private final Supplier<TabModelSelector> mTabModelSelectorProvider;
    private final Supplier<Profile> mProfileSupplier;
    private final ShareSheetDelegate mDelegate;
    private final boolean mIsCustomTab;
    private long mShareStartTime;

    /**
     * Constructs a new {@link ShareDelegateImpl}.
     *
     * @param controller The BottomSheetController for the current activity.
     * @param lifecycleDispatcher Dispatcher for activity lifecycle events, e.g. configuration
     * changes.
     * @param tabProvider Supplier for the current activity tab.
     * @param tabModelSelectorProvider Supplier for the {@link TabModelSelector}. Used to determine
     * whether incognito mode is selected or not.
     * @param profileSupplier Supplier for {@link Profile}.
     * @param delegate The ShareSheetDelegate for the current activity.
     * @param isCustomTab This share delegate is associated with a CCT.
     */
    public ShareDelegateImpl(BottomSheetController controller,
            ActivityLifecycleDispatcher lifecycleDispatcher, Supplier<Tab> tabProvider,
            Supplier<TabModelSelector> tabModelSelectorProvider, Supplier<Profile> profileSupplier,
            ShareSheetDelegate delegate, boolean isCustomTab) {
        mBottomSheetController = controller;
        mLifecycleDispatcher = lifecycleDispatcher;
        mTabProvider = tabProvider;
        mTabModelSelectorProvider = tabModelSelectorProvider;
        mProfileSupplier = profileSupplier;
        mDelegate = delegate;
        mIsCustomTab = isCustomTab;
    }

    // ShareDelegate implementation.
    @Override
    public void share(
            ShareParams params, ChromeShareExtras chromeShareExtras, @ShareOrigin int shareOrigin) {
        if (mShareStartTime == 0L) {
            mShareStartTime = System.currentTimeMillis();
        }
        mDelegate.share(params, chromeShareExtras, mBottomSheetController, mLifecycleDispatcher,
                mTabProvider, mTabModelSelectorProvider, mProfileSupplier, this::printTab,
                shareOrigin, mShareStartTime, isSharingHubEnabled());
        mShareStartTime = 0;
    }

    // ShareDelegate implementation.
    @Override
    public void share(Tab currentTab, boolean shareDirectly, @ShareOrigin int shareOrigin) {
        mShareStartTime = System.currentTimeMillis();
        onShareSelected(currentTab, shareOrigin, shareDirectly);
    }

    /**
     * Triggered when the share menu item is selected.
     * This creates and shows a share intent picker dialog or starts a share intent directly.
     *
     * @param currentTab The current tab.
     * @param shareOrigin Where the share originated.
     * @param shareDirectly Whether it should share directly with the activity that was most
     * recently used to share.
     */
    private void onShareSelected(
            Tab currentTab, @ShareOrigin int shareOrigin, boolean shareDirectly) {
        if (currentTab == null) return;

        triggerShare(currentTab, shareOrigin, shareDirectly);
    }

    private void triggerShare(
            final Tab currentTab, @ShareOrigin final int shareOrigin, final boolean shareDirectly) {
        OfflinePageUtils.maybeShareOfflinePage(currentTab, (ShareParams p) -> {
            if (p != null) {
                share(p, new ChromeShareExtras.Builder().setIsUrlOfVisiblePage(true).build(),
                        shareOrigin);
            } else {
                WindowAndroid window = currentTab.getWindowAndroid();
                // Could not share as an offline page.
                if (shouldFetchCanonicalUrl(currentTab)) {
                    WebContents webContents = currentTab.getWebContents();
                    String title = currentTab.getTitle();
                    GURL visibleUrl = currentTab.getUrl();
                    webContents.getMainFrame().getCanonicalUrlForSharing(new Callback<GURL>() {
                        @Override
                        public void onResult(GURL result) {
                            if (LinkToTextHelper.hasTextFragment(visibleUrl)) {
                                LinkToTextHelper.getExistingSelectorsAllFrames(
                                        currentTab, (selectors) -> {
                                            GURL canonicalUrl =
                                                    new GURL(LinkToTextHelper.getUrlToShare(
                                                            result.getSpec(), selectors));
                                            logCanonicalUrlResult(visibleUrl, canonicalUrl);
                                            triggerShareWithCanonicalUrlResolved(window,
                                                    webContents, title, visibleUrl, canonicalUrl,
                                                    shareOrigin, shareDirectly);
                                        });
                            } else {
                                logCanonicalUrlResult(visibleUrl, result);
                                triggerShareWithCanonicalUrlResolved(window, webContents, title,
                                        visibleUrl, result, shareOrigin, shareDirectly);
                            }
                        }
                    });
                } else {
                    triggerShareWithCanonicalUrlResolved(window, currentTab.getWebContents(),
                            currentTab.getTitle(), currentTab.getUrl(), GURL.emptyGURL(),
                            shareOrigin, shareDirectly);
                }
            }
        });
    }

    private void triggerShareWithCanonicalUrlResolved(final WindowAndroid window,
            final WebContents webContents, final String title, final @NonNull GURL visibleUrl,
            final GURL canonicalUrl, @ShareOrigin final int shareOrigin,
            final boolean shareDirectly) {
        ShareParams.Builder builder =
                new ShareParams.Builder(window, title, getUrlToShare(visibleUrl, canonicalUrl))
                        .setScreenshotUri(null);
        share(builder.build(),
                new ChromeShareExtras.Builder()
                        .setSaveLastUsed(!shareDirectly)
                        .setShareDirectly(shareDirectly)
                        .setIsUrlOfVisiblePage(true)
                        .build(),
                shareOrigin);

        HistoryClustersTabHelper.onCurrentTabUrlShared(webContents);
    }

    @VisibleForTesting
    static boolean shouldFetchCanonicalUrl(final Tab currentTab) {
        WebContents webContents = currentTab.getWebContents();
        if (webContents == null) return false;
        if (webContents.getMainFrame() == null) return false;
        if (currentTab.getUrl().isEmpty()) return false;
        if (currentTab.isShowingErrorPage() || SadTab.isShowing(currentTab)) {
            return false;
        }
        return true;
    }

    private static void logCanonicalUrlResult(GURL visibleUrl, GURL canonicalUrl) {
        @CanonicalURLResult
        int result = getCanonicalUrlResult(visibleUrl, canonicalUrl);
        RecordHistogram.recordEnumeratedHistogram(CANONICAL_URL_RESULT_HISTOGRAM, result,
                CanonicalURLResult.CANONICAL_URL_RESULT_COUNT);
    }

    @VisibleForTesting
    static String getUrlToShare(@NonNull GURL visibleUrl, GURL canonicalUrl) {
        if (canonicalUrl == null || canonicalUrl.isEmpty()) {
            return visibleUrl.getSpec();
        }
        if (!UrlConstants.HTTPS_SCHEME.equals(visibleUrl.getScheme())) {
            return visibleUrl.getSpec();
        }
        if (!UrlUtilities.isHttpOrHttps(canonicalUrl)) {
            return visibleUrl.getSpec();
        }
        return canonicalUrl.getSpec();
    }

    @CanonicalURLResult
    private static int getCanonicalUrlResult(GURL visibleUrl, GURL canonicalUrl) {
        if (!UrlConstants.HTTPS_SCHEME.equals(visibleUrl.getScheme())) {
            return CanonicalURLResult.FAILED_VISIBLE_URL_NOT_HTTPS;
        }
        if (canonicalUrl == null || canonicalUrl.isEmpty()) {
            return CanonicalURLResult.FAILED_NO_CANONICAL_URL_DEFINED;
        }
        String canonicalScheme = canonicalUrl.getScheme();
        if (!UrlConstants.HTTPS_SCHEME.equals(canonicalScheme)) {
            if (!UrlConstants.HTTP_SCHEME.equals(canonicalScheme)) {
                return CanonicalURLResult.FAILED_CANONICAL_URL_INVALID;
            } else {
                return CanonicalURLResult.SUCCESS_CANONICAL_URL_NOT_HTTPS;
            }
        }
        if (visibleUrl.equals(canonicalUrl)) {
            return CanonicalURLResult.SUCCESS_CANONICAL_URL_SAME_AS_VISIBLE;
        } else {
            return CanonicalURLResult.SUCCESS_CANONICAL_URL_DIFFERENT_FROM_VISIBLE;
        }
    }

    private void printTab(Tab tab) {
        Activity activity = mTabProvider.get().getWindowAndroid().getActivity().get();
        PrintingController printingController = PrintingControllerImpl.getInstance();
        if (printingController != null && !printingController.isBusy()) {
            printingController.startPrint(
                    new TabPrinter(mTabProvider.get()), new PrintManagerDelegateImpl(activity));
        }
    }

    @Override
    public boolean isSharingHubEnabled() {
        return !(mIsCustomTab
                || ChromeFeatureList.isEnabled(ChromeFeatureList.SHARE_SHEET_MIGRATION_ANDROID));
    }

    /**
     * Delegate for share handling.
     */
    public static class ShareSheetDelegate {
        /**
         * Trigger the share action for the specified params.
         */
        void share(ShareParams params, ChromeShareExtras chromeShareExtras,
                BottomSheetController controller, ActivityLifecycleDispatcher lifecycleDispatcher,
                Supplier<Tab> tabProvider, Supplier<TabModelSelector> tabModelSelectorSupplier,
                Supplier<Profile> profileSupplier, Callback<Tab> printCallback,
                @ShareOrigin int shareOrigin, long shareStartTime, boolean sharingHubEnabled) {
            Profile profile = profileSupplier.get();
            // In some cases, ProfileSupplier.get() will return null. See https://crbug.com/1346710
            // and https://crbug.com/1353138 for context.
            if (profile == null) {
                profile = Profile.getLastUsedRegularProfile();
            }
            if (chromeShareExtras.shareDirectly()) {
                ShareHelper.shareWithLastUsedComponent(params);
            } else if (sharingHubEnabled && !chromeShareExtras.sharingTabGroup()
                    && profile != null) {
                profile.ensureNativeInitialized();
                // TODO(crbug.com/1085078): Sharing hub is suppressed for tab group sharing.
                // Re-enable it when tab group sharing is supported by sharing hub.
                RecordHistogram.recordEnumeratedHistogram(
                        "Sharing.SharingHubAndroid.Opened", shareOrigin, ShareOrigin.COUNT);
                ShareHelper.recordShareSource(ShareHelper.ShareSourceAndroid.CHROME_SHARE_SHEET);
                boolean isIncognito = tabModelSelectorSupplier.hasValue()
                        && tabModelSelectorSupplier.get().isIncognitoSelected();
                ShareSheetCoordinator coordinator = new ShareSheetCoordinator(controller,
                        lifecycleDispatcher, tabProvider,
                        new ShareSheetPropertyModelBuilder(controller,
                                ContextUtils.getApplicationContext().getPackageManager(), profile),
                        printCallback, new LargeIconBridge(profile), isIncognito,
                        AppHooks.get().getImageEditorModuleProvider(),
                        TrackerFactory.getTrackerForProfile(profile), profile);
                coordinator.showInitialShareSheet(params, chromeShareExtras, shareStartTime);
            } else {
                RecordHistogram.recordEnumeratedHistogram(
                        "Sharing.DefaultSharesheetAndroid.Opened", shareOrigin, ShareOrigin.COUNT);
                // Profile can be null here since it is checked later on before being used.
                ShareHelper.shareWithSystemShareSheetUi(
                        params, profile, chromeShareExtras.saveLastUsed());
            }
        }
    }
}
