// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.pdf.PdfUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.navigation_controller.LoadURLType;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.url.GURL;

/** Java counterpart of android DownloadController. Owned by native. */
@NullMarked
public class DownloadController {
    /**
     * Called to download the given URL triggered from a tab.
     *
     * @param url Url to download.
     * @param tab Tab triggering the download.
     */
    public static void downloadUrl(String url, Tab tab) {
        DownloadControllerJni.get().downloadUrl(url, tab.getWebContents());
    }

    /**
     * Notifies the download delegate that a download completed and passes along info about the
     * download. This can be either a POST download or a GET download with authentication.
     */
    @CalledByNative
    private static void onDownloadCompleted(
            @Nullable Tab tab, DownloadInfo downloadInfo, boolean isDownloadSafe) {
        String fileName = downloadInfo.getFileName();
        String filePath = downloadInfo.getFilePath();
        String mimeType = downloadInfo.getMimeType();
        assert fileName != null && filePath != null && mimeType != null;
        MediaStoreHelper.addImageToGalleryOnSDCard(filePath, mimeType);
        if (tab == null
                || !PdfUtils.shouldOpenPdfInline(tab.isIncognito())
                || !mimeType.equals(MimeTypeUtils.PDF_MIME_TYPE)
                || !downloadInfo.getIsTransient()) {
            return;
        }
        NativePage nativePage = tab.getNativePage();
        if (nativePage == null || !nativePage.isPdf()) {
            return;
        }
        // The PdfPage may become a FrozenNativePage while downloading.
        // Need to check before cast to PdfPage.
        if (nativePage instanceof PdfPage) {
            ((PdfPage) nativePage).onDownloadComplete(fileName, filePath, isDownloadSafe);
            tab.updateTitle();
        }
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
            @JniType("std::string") String userAgent,
            @JniType("std::u16string") String fileName,
            @JniType("std::string") String mimeType,
            @JniType("std::string") String cookie,
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
        void downloadUrl(@JniType("std::string") String url, @Nullable WebContents webContents);

        void cancelDownload(
                @JniType("Profile*") Profile profile,
                @JniType("std::string") @Nullable String downloadGuid);
    }
}
