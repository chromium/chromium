// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.Manifest.permission;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.pdf.PdfUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.download.DownloadCollectionBridge;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.navigation_controller.LoadURLType;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.url.GURL;

/** Java counterpart of android DownloadController. Owned by native. */
public class DownloadController {
    /**
     * Called to download the given URL triggered from a tab.
     *
     * @param url Url to download.
     * @param tab Tab triggering the download.
     */
    public static void downloadUrl(String url, Tab tab) {
        assert hasFileAccess(tab.getWindowAndroid());
        DownloadControllerJni.get().downloadUrl(url, tab.getWebContents());
    }

    /**
     * Notifies the download delegate that a download completed and passes along info about the
     * download. This can be either a POST download or a GET download with authentication.
     */
    @CalledByNative
    private static void onDownloadCompleted(
            @Nullable Tab tab, DownloadInfo downloadInfo, boolean isDownloadSafe) {
        MediaStoreHelper.addImageToGalleryOnSDCard(
                downloadInfo.getFilePath(), downloadInfo.getMimeType());
        if (tab == null
                || !PdfUtils.shouldOpenPdfInline(tab.isIncognito())
                || !downloadInfo.getMimeType().equals(MimeTypeUtils.PDF_MIME_TYPE)) {
            return;
        }
        NativePage nativePage = tab.getNativePage();
        if (nativePage == null || !nativePage.isPdf()) {
            return;
        }
        assert nativePage instanceof PdfPage;
        ((PdfPage) nativePage)
                .onDownloadComplete(
                        downloadInfo.getFileName(), downloadInfo.getFilePath(), isDownloadSafe);
        tab.updateTitle();
    }

    /**
     * Returns whether file access is allowed.
     *
     * @return true if allowed, or false otherwise.
     */
    @CalledByNative
    private static boolean hasFileAccess(WindowAndroid windowAndroid) {
        if (DownloadCollectionBridge.supportsDownloadCollection()) return true;
        AndroidPermissionDelegate delegate = windowAndroid;
        return delegate == null ? false : delegate.hasPermission(permission.WRITE_EXTERNAL_STORAGE);
    }

    /**
     * Requests the storage permission. This should be called from the native code.
     * @param callbackId ID of native callback to notify the result.
     * @param windowAndroid The {@link WindowAndroid} associated with the tab.
     */
    @CalledByNative
    private static void requestFileAccess(final long callbackId, WindowAndroid windowAndroid) {
        if (windowAndroid == null) {
            DownloadControllerJni.get()
                    .onAcquirePermissionResult(
                            callbackId, /* granted= */ false, /* permissionToUpdate= */ "");
            return;
        }
        FileAccessPermissionHelper.requestFileAccessPermissionHelper(
                windowAndroid,
                result -> {
                    DownloadControllerJni.get()
                            .onAcquirePermissionResult(
                                    callbackId,
                                    result.first,
                                    result.second == null ? "" : result.second);
                });
    }

    /**
     * Enqueue a request to download a file using Android DownloadManager.
     *
     * @param url Url to download.
     * @param userAgent User agent to use.
     * @param mimeType MIME type.
     * @param cookie Cookie to use.
     * @param referrer Referrer to use.
     */
    @CalledByNative
    private static void enqueueAndroidDownloadManagerRequest(
            GURL url,
            String userAgent,
            String fileName,
            String mimeType,
            String cookie,
            GURL referrer) {
        DownloadInfo downloadInfo =
                new DownloadInfo.Builder()
                        .setUrl(url)
                        .setUserAgent(userAgent)
                        .setFileName(fileName)
                        .setMimeType(mimeType)
                        .setCookie(cookie)
                        .setReferrer(referrer)
                        .setIsGETRequest(true)
                        .build();
        enqueueDownloadManagerRequest(downloadInfo);
    }

    /**
     * Enqueue a request to download a file using Android DownloadManager.
     *
     * @param info Download information about the download.
     */
    static void enqueueDownloadManagerRequest(final DownloadInfo info) {
        DownloadManagerService.getDownloadManagerService()
                .enqueueNewDownload(new DownloadItem(true, info), true);
    }

    @CalledByNative
    private static void onPdfDownloadStarted(Tab tab, DownloadInfo downloadInfo) {
        if (!PdfUtils.shouldOpenPdfInline(tab.isIncognito())) {
            return;
        }
        String downloadUrl = downloadInfo.getUrl().getSpec();
        String pdfPageUrl = PdfUtils.encodePdfPageUrl(downloadUrl);
        assert pdfPageUrl != null;
        LoadUrlParams param = new LoadUrlParams(pdfPageUrl);
        // Set isPdf param so that other parts of the code can load the pdf native page instead of
        // starting a download.
        param.setIsPdf(true);
        param.setLoadType(LoadURLType.PDF_ANDROID);
        param.setVirtualUrlForSpecialCases(downloadUrl);
        // If the download url matches the tab’s url, avoid duplicate navigation entries by
        // replacing the current entry.
        param.setShouldReplaceCurrentEntry(downloadUrl.equals(tab.getUrl().getSpec()));
        tab.loadUrl(param);
        tab.addObserver(
                new EmptyTabObserver() {
                    @Override
                    public void onDestroyed(Tab tab) {
                        DownloadControllerJni.get()
                                .cancelDownload(tab.getProfile(), downloadInfo.getDownloadGuid());
                    }
                });
    }

    @NativeMethods
    interface Natives {
        void onAcquirePermissionResult(
                long callbackId,
                boolean granted,
                @JniType("std::string") String permissionToUpdate);

        void downloadUrl(@JniType("std::string") String url, WebContents webContents);

        void cancelDownload(
                @JniType("Profile*") Profile profile, @JniType("std::string") String downloadGuid);
    }
}
