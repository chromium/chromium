// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.download;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadMessageUiController;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;

/** Delegate for {@link DownloadMessageUiController} to provide chrome layer dependencies. */
public class DownloadMessageUiDelegate implements DownloadMessageUiController.Delegate {
    private WeakReference<ChromeActivity> mActivity = new WeakReference<ChromeActivity>(null);

    /** Constructor. */
    public DownloadMessageUiDelegate() {
        maybeSwitchToFocusedActivity();
    }

    @Override
    public @Nullable Context getContext() {
        return mActivity.get();
    }

    @Override
    public @Nullable MessageDispatcher getMessageDispatcher() {
        ChromeActivity chromeActivity = mActivity.get();
        if (chromeActivity == null) return null;

        WindowAndroid windowAndroid = chromeActivity.getWindowAndroid();
        if (windowAndroid == null) return null;

        return MessageDispatcherProvider.from(windowAndroid);
    }

    @Override
    public @Nullable ModalDialogManager getModalDialogManager() {
        ChromeActivity chromeActivity = mActivity.get();
        return chromeActivity == null ? null : chromeActivity.getModalDialogManager();
    }

    @Override
    public boolean maybeSwitchToFocusedActivity() {
        Activity focusedActivity = ApplicationStatus.getLastTrackedFocusedActivity();
        boolean shouldSwitchToFocusedActivity =
                focusedActivity instanceof ChromeActivity && focusedActivity != mActivity.get();
        if (!shouldSwitchToFocusedActivity) return false;
        mActivity = new WeakReference<ChromeActivity>((ChromeActivity) focusedActivity);
        return true;
    }

    @Override
    public void openDownloadsPage(OTRProfileID otrProfileID, int source) {
        DownloadManagerService.openDownloadsPage(otrProfileID, source);
    }

    @Override
    public void openDownload(
            ContentId contentId, OTRProfileID otrProfileID, int source, Context context) {
        DownloadUtils.openItem(contentId, otrProfileID, source, context);
    }

    @Override
    public void removeNotification(int notificationId, DownloadInfo downloadInfo) {
        DownloadManagerService.getDownloadManagerService()
                .getDownloadNotifier()
                .removeDownloadNotification(notificationId, downloadInfo);
    }
}
