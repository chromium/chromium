// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.text.style.ClickableSpan;
import android.view.View;

import org.jni_zero.CalledByNative;

import org.chromium.base.CallbackUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadOpenSource;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.DuplicateDownloadClickableSpan;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.components.infobars.ConfirmInfoBar;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarLayout;

/**
 * An infobar to ask whether to proceed downloading a file that already exists locally or is still
 * being downloaded.
 */
public class DuplicateDownloadInfoBar extends ConfirmInfoBar {
    private final String mFilePath;
    private final boolean mIsOfflinePage;
    private final String mPageUrl;
    private final OTRProfileID mOTRProfileID;
    private final boolean mDuplicateRequestExists;

    @CalledByNative
    private static InfoBar createInfoBar(
            String filePath,
            boolean isOfflinePage,
            String pageUrl,
            OTRProfileID otrProfileID,
            boolean duplicateRequestExists) {
        return new DuplicateDownloadInfoBar(
                ContextUtils.getApplicationContext(),
                filePath,
                isOfflinePage,
                pageUrl,
                otrProfileID,
                duplicateRequestExists);
    }

    /**
     * Constructs DuplicateDownloadInfoBar.
     * @param context Application context.
     * @param filePath The file path.
     * @param isOfflinePage Whether the download is for offline page.
     * @param pageUrl Url of the page, ignored if this is a regular download.
     * @param otrProfileID The {@link OTRProfileID} of the download. Null if in regular mode.
     * @param duplicateRequestExists Whether the duplicate is a download in progress.
     */
    private DuplicateDownloadInfoBar(
            Context context,
            String filePath,
            boolean isOfflinePage,
            String pageUrl,
            OTRProfileID otrProfileID,
            boolean duplicateRequestExists) {
        super(
                R.drawable.infobar_downloading,
                R.color.infobar_icon_drawable_color,
                null,
                null,
                null,
                context.getString(R.string.duplicate_download_infobar_download_button),
                context.getString(R.string.cancel));
        mFilePath = filePath;
        mIsOfflinePage = isOfflinePage;
        mPageUrl = pageUrl;
        mOTRProfileID = otrProfileID;
        mDuplicateRequestExists = duplicateRequestExists;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        Context context = layout.getContext();
        if (mIsOfflinePage) {
            layout.setMessage(
                    DownloadUtils.getOfflinePageMessageText(
                            context,
                            mFilePath,
                            mDuplicateRequestExists,
                            new ClickableSpan() {
                                @Override
                                public void onClick(View view) {
                                    DownloadUtils.openPageUrl(context, mPageUrl);
                                }
                            }));
        } else {
            DuplicateDownloadClickableSpan span =
                    new DuplicateDownloadClickableSpan(
                            context,
                            mFilePath,
                            CallbackUtils.emptyRunnable(),
                            mOTRProfileID,
                            DownloadOpenSource.INFO_BAR);
            layout.setMessage(
                    DownloadUtils.getDownloadMessageText(
                            context,
                            context.getString(R.string.duplicate_download_infobar_text),
                            mFilePath,
                            /* addSizeStringIfAvailable= */ false,
                            /* totalBytes= */ 0,
                            span));
        }
    }
}
