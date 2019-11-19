// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.feature_engagement.ScreenshotTabObserver;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.printing.PrintShareActivity;
import org.chromium.chrome.browser.send_tab_to_self.SendTabToSelfShareActivity;
import org.chromium.chrome.browser.share.qrcode.QrCodeShareActivity;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ChromeFileProvider;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.components.ui_metrics.CanonicalURLResult;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.GURLUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/**
 * Handles displaying the share sheet. The version used depends on several
 * conditions.
 * Android K and below: custom share dialog
 * Android L+: system share sheet
 * #chrome-sharing-hub enabled: custom share sheet
 */
class ShareSheetMediator {
    static final String CANONICAL_URL_RESULT_HISTOGRAM = "Mobile.CanonicalURLResult";
    private static boolean sScreenshotCaptureSkippedForTesting;
    private final ShareSheetDelegate mDelegate;
    private final BottomSheetController mBottomSheetController;

    @VisibleForTesting
    ShareSheetMediator(ShareSheetDelegate delegate, BottomSheetController controller) {
        mDelegate = delegate;
        mBottomSheetController = controller;
    }

    /**
     * Triggered when the share menu item is selected.
     * This creates and shows a share intent picker dialog or starts a share intent directly.
     * @param shareDirectly Whether it should share directly with the activity that was most
     *                      recently used to share.
     * @param isIncognito Whether currentTab is incognito.
     */
    public void onShareSelected(
            Activity activity, Tab currentTab, boolean shareDirectly, boolean isIncognito) {
        if (currentTab == null) return;

        List<Class<? extends ShareActivity>> classesToEnable = new ArrayList<>(2);

        if (PrintShareActivity.featureIsAvailable(currentTab)) {
            classesToEnable.add(PrintShareActivity.class);
        }

        if (SendTabToSelfShareActivity.featureIsAvailable(currentTab)) {
            classesToEnable.add(SendTabToSelfShareActivity.class);
        }

        if (QrCodeShareActivity.featureIsAvailable()) {
            classesToEnable.add(QrCodeShareActivity.class);
        }

        if (!classesToEnable.isEmpty()) {
            OptionalShareTargetsManager.getInstance().enableOptionalShareActivities(activity,
                    classesToEnable, () -> triggerShare(currentTab, shareDirectly, isIncognito));
            return;
        }

        triggerShare(currentTab, shareDirectly, isIncognito);
    }

    /**
     * Creates and shows a share intent picker dialog or starts a share intent directly with the
     * activity that was most recently used to share based on shareDirectly value.
     *
     * This function will save |screenshot| under {app's root}/files/images/screenshot (or
     * /sdcard/DCIM/browser-images/screenshot if ADK is lower than JB MR2).
     * Cleaning up doesn't happen automatically, and so an app should call clearSharedScreenshots()
     * explicitly when needed.
     *
     * @param params The container holding the share parameters.
     */
    public void share(ShareParams params) {
        mDelegate.share(params, mBottomSheetController);
    }

    /**
     * Creates and shows a custom share sheet.
     *
     * @param params The container holding the share parameters.
     */
    private static void showShareSheet(final ShareParams params, BottomSheetController controller) {
        controller.requestShowContent(
                (new ShareSheetBottomSheetContent(params.getWindow().getContext().get())), true);
    }

    protected void triggerShare(
            final Tab currentTab, final boolean shareDirectly, boolean isIncognito) {
        ScreenshotTabObserver tabObserver = ScreenshotTabObserver.from(currentTab);
        if (tabObserver != null) {
            tabObserver.onActionPerformedAfterScreenshot(
                    ScreenshotTabObserver.SCREENSHOT_ACTION_SHARE);
        }

        OfflinePageUtils.maybeShareOfflinePage(currentTab, (ShareParams p) -> {
            if (p != null) {
                share(p);
            } else {
                WindowAndroid window = currentTab.getWindowAndroid();
                // Could not share as an offline page.
                if (shouldFetchCanonicalUrl(currentTab)) {
                    WebContents webContents = currentTab.getWebContents();
                    String title = currentTab.getTitle();
                    String visibleUrl = currentTab.getUrl();
                    webContents.getMainFrame().getCanonicalUrlForSharing(new Callback<String>() {
                        @Override
                        public void onResult(String result) {
                            logCanonicalUrlResult(visibleUrl, result);

                            triggerShareWithCanonicalUrlResolved(window, webContents, title,
                                    visibleUrl, result, shareDirectly, isIncognito);
                        }
                    });
                } else {
                    triggerShareWithCanonicalUrlResolved(window, currentTab.getWebContents(),
                            currentTab.getTitle(), currentTab.getUrl(), null, shareDirectly,
                            isIncognito);
                }
            }
        });
    }

    private void triggerShareWithCanonicalUrlResolved(final WindowAndroid window,
            final WebContents webContents, final String title, final String visibleUrl,
            final String canonicalUrl, final boolean shareDirectly, boolean isIncognito) {
        // Share an empty blockingUri in place of screenshot file. The file ready notification is
        // sent by onScreenshotReady call below when the file is written.
        final Uri blockingUri = (isIncognito || webContents == null)
                ? null
                : ChromeFileProvider.generateUriAndBlockAccess();
        ShareParams.Builder builder =
                new ShareParams.Builder(window, title, getUrlToShare(visibleUrl, canonicalUrl))
                        .setShareDirectly(shareDirectly)
                        .setSaveLastUsed(!shareDirectly)
                        .setScreenshotUri(blockingUri);
        share(builder.build());
        if (shareDirectly) {
            RecordUserAction.record("MobileMenuDirectShare");
        } else {
            RecordUserAction.record("MobileMenuShare");
        }

        if (blockingUri == null) return;

        // Start screenshot capture and notify the provider when it is ready.
        Callback<Uri> callback = (saveFile) -> {
            // Unblock the file once it is saved to disk.
            ChromeFileProvider.notifyFileReady(blockingUri, saveFile);
        };
        if (sScreenshotCaptureSkippedForTesting) {
            callback.onResult(null);
        } else {
            ShareHelper.captureScreenshotForContents(webContents, 0, 0, callback);
        }
    }

    @VisibleForTesting
    static boolean shouldFetchCanonicalUrl(final Tab currentTab) {
        WebContents webContents = currentTab.getWebContents();
        if (webContents == null) return false;
        if (webContents.getMainFrame() == null) return false;
        String url = currentTab.getUrl();
        if (TextUtils.isEmpty(url)) return false;
        if (currentTab.isShowingErrorPage() || currentTab.isShowingInterstitialPage()
                || SadTab.isShowing(currentTab)) {
            return false;
        }
        return true;
    }

    private static void logCanonicalUrlResult(String visibleUrl, String canonicalUrl) {
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
    static String getUrlToShare(String visibleUrl, String canonicalUrl) {
        if (TextUtils.isEmpty(canonicalUrl)) return visibleUrl;
        // TODO(tedchoc): Can we replace GURLUtils.getScheme with Uri.parse(...).getScheme()
        //                https://crbug.com/783819
        if (!UrlConstants.HTTPS_SCHEME.equals(GURLUtils.getScheme(visibleUrl))) {
            return visibleUrl;
        }
        String canonicalScheme = GURLUtils.getScheme(canonicalUrl);
        if (!UrlConstants.HTTP_SCHEME.equals(canonicalScheme)
                && !UrlConstants.HTTPS_SCHEME.equals(canonicalScheme)) {
            return visibleUrl;
        }
        return canonicalUrl;
    }

    @CanonicalURLResult
    private static int getCanonicalUrlResult(String visibleUrl, String canonicalUrl) {
        if (!UrlConstants.HTTPS_SCHEME.equals(GURLUtils.getScheme(visibleUrl))) {
            return CanonicalURLResult.FAILED_VISIBLE_URL_NOT_HTTPS;
        }
        if (TextUtils.isEmpty(canonicalUrl)) {
            return CanonicalURLResult.FAILED_NO_CANONICAL_URL_DEFINED;
        }
        String canonicalScheme = GURLUtils.getScheme(canonicalUrl);
        if (!UrlConstants.HTTPS_SCHEME.equals(canonicalScheme)) {
            if (!UrlConstants.HTTP_SCHEME.equals(canonicalScheme)) {
                return CanonicalURLResult.FAILED_CANONICAL_URL_INVALID;
            } else {
                return CanonicalURLResult.SUCCESS_CANONICAL_URL_NOT_HTTPS;
            }
        }
        if (TextUtils.equals(visibleUrl, canonicalUrl)) {
            return CanonicalURLResult.SUCCESS_CANONICAL_URL_SAME_AS_VISIBLE;
        } else {
            return CanonicalURLResult.SUCCESS_CANONICAL_URL_DIFFERENT_FROM_VISIBLE;
        }
    }

    /**
     * Delegate for share handling.
     */
    static class ShareSheetDelegate {
        /**
         * Trigger the share action for the specified params.
         */
        void share(ShareParams params, BottomSheetController controller) {
            if (params.shareDirectly()) {
                ShareHelper.shareDirectly(params);
            } else if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB)) {
                // TODO(crbug/1009124): open custom share sheet.
                showShareSheet(params, controller);
            } else if (ShareHelper.TargetChosenReceiver.isSupported()) {
                // On L+ open system share sheet.
                ShareHelper.makeIntentAndShare(params, null);
            } else {
                // On K and below open custom share dialog.
                ShareHelper.showShareDialog(params);
            }
        }
    }
}
