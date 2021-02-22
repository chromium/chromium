// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.feature_engagement.ScreenshotTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.printing.PrintShareActivity;
import org.chromium.chrome.browser.printing.TabPrinter;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.send_tab_to_self.SendTabToSelfShareActivity;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetPropertyModelBuilder;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ChromeFileProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
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

import java.util.ArrayList;
import java.util.List;

/**
 * Implementation of share interface. Mostly a wrapper around ShareSheetCoordinator.
 */
public class ShareDelegateImpl implements ShareDelegate {
    static final String CANONICAL_URL_RESULT_HISTOGRAM = "Mobile.CanonicalURLResult";

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({ShareOrigin.OVERFLOW_MENU, ShareOrigin.TOP_TOOLBAR, ShareOrigin.CONTEXT_MENU,
            ShareOrigin.WEBSHARE_API, ShareOrigin.MOBILE_ACTION_MODE, ShareOrigin.EDIT_URL,
            ShareOrigin.TAB_GROUP, ShareOrigin.WEBAPP_NOTIFICATION, ShareOrigin.FEED})
    public @interface ShareOrigin {
        int OVERFLOW_MENU = 0;
        int TOP_TOOLBAR = 1;
        int CONTEXT_MENU = 2;
        int WEBSHARE_API = 3;
        int MOBILE_ACTION_MODE = 4;
        int EDIT_URL = 5;
        int TAB_GROUP = 6;
        int WEBAPP_NOTIFICATION = 7;
        int FEED = 8;

        // Must be the last one.
        int COUNT = 9;
    }

    private final BottomSheetController mBottomSheetController;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final Supplier<Tab> mTabProvider;
    private final ShareSheetDelegate mDelegate;
    private final boolean mIsCustomTab;
    private long mShareStartTime;

    private static boolean sScreenshotCaptureSkippedForTesting;

    /**
     * Constructs a new {@link ShareDelegateImpl}.
     *
     * @param controller The BottomSheetController for the current activity.
     * @param lifecycleDispatcher Dispatcher for activity lifecycle events, e.g. configuration
     * changes.
     * @param tabProvider Supplier for the current activity tab.
     * @param delegate The ShareSheetDelegate for the current activity.
     * @param isCustomTab This share delegate is associated with a CCT.
     */
    public ShareDelegateImpl(BottomSheetController controller,
            ActivityLifecycleDispatcher lifecycleDispatcher, Supplier<Tab> tabProvider,
            ShareSheetDelegate delegate, boolean isCustomTab) {
        mBottomSheetController = controller;
        mLifecycleDispatcher = lifecycleDispatcher;
        mTabProvider = tabProvider;
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
        boolean isSyncEnabled =
                ProfileSyncService.get() != null && ProfileSyncService.get().isSyncRequested();
        mDelegate.share(params, chromeShareExtras, mBottomSheetController, mLifecycleDispatcher,
                mTabProvider, this::printTab, shareOrigin, isSyncEnabled, mShareStartTime,
                isSharingHubV1Enabled());
        mShareStartTime = 0;
    }

    // ShareDelegate implementation.
    @Override
    public void share(Tab currentTab, boolean shareDirectly, @ShareOrigin int shareOrigin) {
        mShareStartTime = System.currentTimeMillis();
        onShareSelected(currentTab.getWindowAndroid().getActivity().get(), currentTab, shareOrigin,
                shareDirectly, currentTab.isIncognito());
    }

    /**
     * Triggered when the share menu item is selected.
     * This creates and shows a share intent picker dialog or starts a share intent directly.
     *
     * @param shareOrigin Where the share originated.
     * @param shareDirectly Whether it should share directly with the activity that was most
     * recently used to share.
     * @param isIncognito Whether currentTab is incognito.
     */
    private void onShareSelected(Activity activity, Tab currentTab, @ShareOrigin int shareOrigin,
            boolean shareDirectly, boolean isIncognito) {
        if (currentTab == null) return;

        List<Class<? extends Activity>> classesToEnable = new ArrayList<>(2);

        if (PrintShareActivity.featureIsAvailable(currentTab)) {
            classesToEnable.add(PrintShareActivity.class);
        }

        if (SendTabToSelfShareActivity.featureIsAvailable(currentTab)) {
            classesToEnable.add(SendTabToSelfShareActivity.class);
        }

        if (!classesToEnable.isEmpty()) {
            OptionalShareTargetsManager.getInstance().enableOptionalShareActivities(activity,
                    classesToEnable,
                    () -> triggerShare(currentTab, shareOrigin, shareDirectly, isIncognito));
            return;
        }

        triggerShare(currentTab, shareOrigin, shareDirectly, isIncognito);
    }

    private void triggerShare(final Tab currentTab, @ShareOrigin final int shareOrigin,
            final boolean shareDirectly, boolean isIncognito) {
        ScreenshotTabObserver tabObserver = ScreenshotTabObserver.from(currentTab);
        if (tabObserver != null) {
            tabObserver.onActionPerformedAfterScreenshot(
                    ScreenshotTabObserver.SCREENSHOT_ACTION_SHARE);
        }

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
                            logCanonicalUrlResult(visibleUrl, result);

                            triggerShareWithCanonicalUrlResolved(window, webContents, title,
                                    visibleUrl, result, shareOrigin, shareDirectly, isIncognito);
                        }
                    });
                } else {
                    triggerShareWithCanonicalUrlResolved(window, currentTab.getWebContents(),
                            currentTab.getTitle(), currentTab.getUrl(), GURL.emptyGURL(),
                            shareOrigin, shareDirectly, isIncognito);
                }
            }
        });
    }

    private void triggerShareWithCanonicalUrlResolved(final WindowAndroid window,
            final WebContents webContents, final String title, final @NonNull GURL visibleUrl,
            final GURL canonicalUrl, @ShareOrigin final int shareOrigin,
            final boolean shareDirectly, boolean isIncognito) {
        // Share an empty blockingUri in place of screenshot file. The file ready notification is
        // sent by onScreenshotReady call below when the file is written.
        final Uri blockingUri = (isIncognito || webContents == null)
                ? null
                : ChromeFileProvider.generateUriAndBlockAccess();
        ShareParams.Builder builder =
                new ShareParams.Builder(window, title, getUrlToShare(visibleUrl, canonicalUrl))
                        .setScreenshotUri(blockingUri);
        share(builder.build(),
                new ChromeShareExtras.Builder()
                        .setSaveLastUsed(!shareDirectly)
                        .setShareDirectly(shareDirectly)
                        .setIsUrlOfVisiblePage(true)
                        .build(),
                shareOrigin);

        if (blockingUri == null) return;

        // Start screenshot capture and notify the provider when it is ready.
        Callback<Uri> callback = (saveFile) -> {
            // Unblock the file once it is saved to disk.
            ChromeFileProvider.notifyFileReady(blockingUri, saveFile);
        };
        if (sScreenshotCaptureSkippedForTesting) {
            callback.onResult(null);
        } else {
            ShareImageFileUtils.captureScreenshotForContents(webContents, 0, 0, callback);
        }
    }

    @VisibleForTesting
    static boolean shouldFetchCanonicalUrl(final Tab currentTab) {
        WebContents webContents = currentTab.getWebContents();
        if (webContents == null) return false;
        if (webContents.getMainFrame() == null) return false;
        String url = currentTab.getUrlString();
        if (TextUtils.isEmpty(url)) return false;
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
    public static void setScreenshotCaptureSkippedForTesting(boolean value) {
        sScreenshotCaptureSkippedForTesting = value;
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
    public boolean isSharingHubV1Enabled() {
        return !mIsCustomTab && ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB);
    }

    @Override
    public boolean isSharingHubV15Enabled() {
        return isSharingHubV1Enabled()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB_V15);
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
                Supplier<Tab> tabProvider, Callback<Tab> printCallback,
                @ShareOrigin int shareOrigin, boolean isSyncEnabled, long shareStartTime,
                boolean sharingHubEnabled) {
            if (chromeShareExtras.shareDirectly()) {
                ShareHelper.shareWithLastUsedComponent(params);
            } else if (sharingHubEnabled && !chromeShareExtras.sharingTabGroup()
                    && tabProvider.get() != null) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Sharing.SharingHubAndroid.Opened", shareOrigin, ShareOrigin.COUNT);
                // TODO(crbug.com/1085078): Sharing hub is suppressed for tab group sharing.
                // Re-enable it when tab group sharing is supported by sharing hub.
                ShareSheetCoordinator coordinator = new ShareSheetCoordinator(controller,
                        lifecycleDispatcher, tabProvider,
                        new ShareSheetPropertyModelBuilder(controller,
                                ContextUtils.getApplicationContext().getPackageManager()),
                        printCallback, new LargeIconBridge(Profile.getLastUsedRegularProfile()),
                        new SettingsLauncherImpl(), isSyncEnabled,
                        AppHooks.get().getImageEditorModuleProvider(),
                        TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile()));
                // TODO(crbug/1009124): open custom share sheet.
                coordinator.showInitialShareSheet(params, chromeShareExtras, shareStartTime);
            } else {
                RecordHistogram.recordEnumeratedHistogram(
                        "Sharing.DefaultSharesheetAndroid.Opened", shareOrigin, ShareOrigin.COUNT);
                ShareHelper.showDefaultShareUi(params, chromeShareExtras.saveLastUsed());
            }
        }
    }
}
