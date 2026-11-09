// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.StrictModeContext;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.pdf.PdfUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.navigation_controller.LoadURLType;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.io.File;

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
     * Downloads a local file owned by Chrome (a file:// or content:// URI) through the regular
     * download flow: target determination, location/duplicate dialogs, download UI and Download
     * Home, exactly like a network download.
     *
     * <p>A {@code file://} source is re-pointed at Chrome's content providers when covered so a
     * file already in the Downloads directory goes through the standard duplicate-download dialog /
     * uniquification flow instead of failing as a same-path self-download; uncovered {@code
     * file://} paths are passed through unchanged.
     *
     * @param uri The file:// or content:// URI of the local file.
     * @param suggestedFileName The file name (with extension) to save the download as.
     * @param tab The tab that initiated the download.
     */
    public static void downloadLocalFile(String uri, @Nullable String suggestedFileName, Tab tab) {
        // A file:// source is re-pointed at Chrome's content providers (DownloadFileProvider /
        // FileProvider) before reaching the download manager.
        // DownloadPathReservationTracker only derives |source_path| from file:// URLs, so a
        // file:// that already lives in the Downloads directory would otherwise be rejected
        // with DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE instead of going through the
        // duplicate-download dialog / uniquification like any other download.
        Uri parsed = Uri.parse(uri);
        String path = parsed.getPath();
        if (UrlConstants.FILE_SCHEME.equals(parsed.getScheme())
                && path != null
                && !path.isEmpty()) {
            Uri resolved;
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                resolved = DownloadUtils.getUriForItem(path);
            }
            if (UrlConstants.CONTENT_SCHEME.equals(resolved.getScheme())
                    && TextUtils.isEmpty(suggestedFileName)) {
                suggestedFileName = new File(path).getName();
            }
            uri = resolved.toString(); // falls back to file:// if uncovered
        }
        DownloadControllerJni.get()
                .downloadLocalFile(
                        uri,
                        suggestedFileName != null ? suggestedFileName : "",
                        tab.getWebContents());
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
        if (nativePage instanceof PdfPage pdfPage) {
            pdfPage.onDownloadComplete(fileName, filePath, isDownloadSafe);
            tab.updateTitle();
        }
    }

    /**
     * Notifies the pdf native page that its inline pdf download failed, so the user can be
     * informed.
     */
    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void onDownloadFailed(@Nullable Tab tab, DownloadInfo downloadInfo) {
        String mimeType = downloadInfo.getMimeType();
        if (tab == null
                || mimeType == null
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
        if (nativePage instanceof PdfPage pdfPage) {
            pdfPage.onDownloadFailed();
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
            @JniType("GURL") GURL url,
            @JniType("std::string") String userAgent,
            @JniType("std::u16string") String fileName,
            @JniType("std::string") String mimeType,
            @JniType("std::string") String cookie,
            @JniType("GURL") GURL referrer) {
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

    /**
     * Called when an inline PDF download starts, to show the pdf native page for it.
     *
     * @param tab Tab the download was triggered from.
     * @param downloadInfo Information about the download.
     * @param initiatorOrigin Origin that initiated the download, or null if the download was
     *     browser-initiated (e.g. from the omnibox, a bookmark or an external intent).
     */
    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void onPdfDownloadStarted(
            Tab tab,
            DownloadInfo downloadInfo,
            @JniType("std::optional<url::Origin>") @Nullable Origin initiatorOrigin) {
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
        // Record the initiator so that the re-download triggered by a later history traversal or
        // tab restore keeps the SameSite context of the navigation that caused this download.
        param.setInitiatorOrigin(initiatorOrigin);
        // If the download url matches the tab’s url, avoid duplicate navigation entries by
        // replacing the current entry.
        param.setShouldReplaceCurrentEntry(downloadUrl.equals(tab.getUrl().getSpec()));
        tab.loadUrl(param);
        tab.addObserver(
                new TabObserver() {
                    @Override
                    public void onDestroyed(Tab tab) {
                        DownloadControllerJni.get()
                                .cancelDownload(tab.getProfile(), downloadInfo.getDownloadGuid());
                    }
                });
    }

    @NativeMethods
    interface Natives {
        void downloadUrl(
                @JniType("std::string") String url,
                @JniType("content::WebContents*") @Nullable WebContents webContents);

        void downloadLocalFile(
                @JniType("std::string") String url,
                @JniType("std::u16string") String suggestedFileName,
                @JniType("content::WebContents*") @Nullable WebContents webContents);

        void cancelDownload(
                @JniType("Profile*") Profile profile,
                @JniType("std::string") @Nullable String downloadGuid);
    }
}
