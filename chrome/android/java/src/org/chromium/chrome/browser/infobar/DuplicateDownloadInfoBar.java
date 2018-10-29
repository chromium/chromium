// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.content.Intent;
import android.graphics.Typeface;
import android.net.Uri;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.ClickableSpan;
import android.text.style.StyleSpan;
import android.view.View;
import android.webkit.MimeTypeMap;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadMetrics;
import org.chromium.chrome.browser.download.DownloadUtils;

import java.io.File;

/**
 * An infobar to ask whether to proceed downloading a file that already exists locally or is still
 * being downloaded.
 */
public class DuplicateDownloadInfoBar extends ConfirmInfoBar {
    private static final String TAG = "DuplicateDownloadInfoBar";
    private final String mFilePath;
    private final boolean mIsOfflinePage;
    private final String mPageUrl;
    private final boolean mIsIncognito;
    private final boolean mDuplicateRequestExists;

    @CalledByNative
    private static InfoBar createInfoBar(String filePath, boolean isOfflinePage, String pageUrl,
            boolean isIncognito, boolean duplicateRequestExists) {
        return new DuplicateDownloadInfoBar(ContextUtils.getApplicationContext(), filePath,
                isOfflinePage, pageUrl, isIncognito, duplicateRequestExists);
    }

    /**
     * Constructs DuplicateDownloadInfoBar.
     * @param context Application context.
     * @param filePath The file path.
     * @param isOfflinePage Whether the download is for offline page.
     * @param pageUrl Url of the page, ignored if this is a regular download.
     * @param isIncognito Whether download is Incognito.
     * @param duplicateRequestExists Whether the duplicate is a download in progress.
     */
    private DuplicateDownloadInfoBar(Context context, String filePath, boolean isOfflinePage,
            String pageUrl, boolean isIncognito, boolean duplicateRequestExists) {
        super(R.drawable.infobar_downloading, null, null, null,
                context.getString(R.string.duplicate_download_infobar_download_button),
                context.getString(R.string.cancel));
        mFilePath = filePath;
        mIsOfflinePage = isOfflinePage;
        mPageUrl = pageUrl;
        mIsIncognito = isIncognito;
        mDuplicateRequestExists = duplicateRequestExists;
    }

    /**
     * Gets the infobar text for regular downloads.
     * @param context Context to be used.
     * @param template Template of the text to be displayed.
     */
    private CharSequence getDownloadMessageText(final Context context, final String template) {
        final File file = new File(mFilePath);
        final Uri fileUri = Uri.fromFile(file);
        final String mimeType = getMimeTypeFromUri(fileUri);
        final String filename = file.getName();
        return getMessageText(template, filename, new ClickableSpan() {
            @Override
            public void onClick(View view) {
                new AsyncTask<Boolean>() {
                    @Override
                    protected Boolean doInBackground() {
                        return new File(mFilePath).exists();
                    }

                    @Override
                    protected void onPostExecute(Boolean fileExists) {
                        if (fileExists) {
                            DownloadUtils.openFile(file, mimeType, null, mIsIncognito, null, null,
                                    DownloadMetrics.DownloadOpenSource.INFO_BAR);
                        } else {
                            DownloadManagerService.openDownloadsPage(
                                    ContextUtils.getApplicationContext());
                        }
                    }
                }
                        .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            }
        });
    }

    /**
     * Gets the infobar text for offline page downloads.
     * @param context Context to be used.
     * @param template Template of the text to be displayed.
     */
    private CharSequence getOfflinePageMessageText(final Context context, final String template) {
        return getMessageText(template, mFilePath, new ClickableSpan() {
            @Override
            public void onClick(View view) {
                // TODO(qinmin): open the offline page on local storage instead of opening the url.
                // However, there could be multiple stored offline pages for the same url, need to
                // figure out which one to use.
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setData(Uri.parse(mPageUrl));
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                intent.setPackage(context.getPackageName());
                context.startActivity(intent);
            }
        });
    }

    /**
     * Helper method to get the text to be displayed on the infobar.
     * @param template Message template.
     * @param fileName Name of the file.
     * @param clickableSpan Action to perform when clicking on the file name.
     * @return message to be displayed on the infobar.
     */
    private CharSequence getMessageText(
            final String template, final String fileName, final ClickableSpan clickableSpan) {
        final SpannableString formattedFilePath = new SpannableString(fileName);
        formattedFilePath.setSpan(new StyleSpan(Typeface.BOLD), 0, fileName.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        formattedFilePath.setSpan(
                clickableSpan, 0, fileName.length(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        return TextUtils.expandTemplate(template, formattedFilePath);
    }

    /**
     * Retrieve the mime type based on the given file URI.
     * @param fileUri URI of the file
     * @return Possible mime type of the file.
     */
    private static String getMimeTypeFromUri(Uri fileUri) {
        String extension = MimeTypeMap.getSingleton().getFileExtensionFromUrl(fileUri.toString());
        return MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        Context context = layout.getContext();
        String template = context.getString(mDuplicateRequestExists
                        ? R.string.duplicate_download_request_infobar_text
                        : R.string.duplicate_download_infobar_text);
        if (mIsOfflinePage) {
            layout.setMessage(getOfflinePageMessageText(context, template));
        } else {
            layout.setMessage(getDownloadMessageText(context, template));
        }
    }
}
