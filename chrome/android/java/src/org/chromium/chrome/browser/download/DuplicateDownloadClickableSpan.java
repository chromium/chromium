// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.net.Uri;
import android.text.style.ClickableSpan;
import android.view.View;
import android.webkit.MimeTypeMap;

import androidx.annotation.Nullable;

import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.components.download.DownloadCollectionBridge;

import java.io.File;

/**
 * Class for opening a download file when clicking a file name on a duplicate download dialog or
 * infobar.
 */
public class DuplicateDownloadClickableSpan extends ClickableSpan {
    private final @Nullable Runnable mRunnable;
    private final OtrProfileId mOtrProfileId;
    private final String mFilePath;
    private @DownloadOpenSource int mSource;

    /**
     * Constructor.
     *
     * @param filePath file path of the download files.
     * @param runnable Runnable that will be executed when clicking the file name.
     * @param otrProfileId Off the record profile ID.
     * @param source Enum for UMA reporting.
     */
    public DuplicateDownloadClickableSpan(
            String filePath,
            Runnable runnable,
            OtrProfileId otrProfileId,
            @DownloadOpenSource int source) {
        mRunnable = runnable;
        mOtrProfileId = otrProfileId;
        mFilePath = filePath;
        mSource = source;
    }

    private class ClickableSpanAsyncTask extends AsyncTask<String> {
        private String mMimeType;

        @Override
        protected String doInBackground() {
            File file = new File(mFilePath);
            if (DownloadCollectionBridge.shouldPublishDownload(mFilePath)) {
                Uri uri = DownloadCollectionBridge.getDownloadUriForFileName(file.getName());
                mMimeType = getMimeTypeFromUri(Uri.fromFile(file));
                return uri == null ? null : uri.toString();
            } else {
                if (file.exists()) return mFilePath;
                return null;
            }
        }

        @Override
        protected void onPostExecute(String filePath) {
            if (mRunnable != null) mRunnable.run();
            if (filePath != null) {
                DownloadUtils.openDownload(
                        filePath, mMimeType, null, mOtrProfileId, null, null, mSource);
            } else {
                DownloadManagerService.openDownloadsPage(mOtrProfileId, mSource);
            }
        }
    }

    @Override
    public void onClick(View view) {
        new ClickableSpanAsyncTask().executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Retrieve the mime type based on the given file URI.
     * @param fileUri URI of the file
     * @return Possible mime type of the file.
     */
    private static String getMimeTypeFromUri(Uri fileUri) {
        String extension = MimeTypeMap.getFileExtensionFromUrl(fileUri.toString());
        return MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
    }
}
