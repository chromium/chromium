// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.share_tab;

import android.content.Context;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.share.qrcode.QrCodeDialogTab;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Creates and represents the QrCode share panel UI. */
public class QrCodeShareCoordinator implements QrCodeDialogTab {
    private final QrCodeShareView mShareView;
    private final QrCodeShareMediator mMediator;

    public QrCodeShareCoordinator(
            Context context, Runnable closeDialog, String url, WindowAndroid windowAndroid) {
        PropertyModel shareViewModel = new PropertyModel(QrCodeShareViewProperties.ALL_KEYS);
        mMediator =
                new QrCodeShareMediator(context, shareViewModel, closeDialog, url, windowAndroid);
        mShareView = new QrCodeShareView(context, mMediator::downloadQrCode);
        PropertyModelChangeProcessor.create(
                shareViewModel, mShareView, new QrCodeShareViewBinder());
    }

    /** QrCodeDialogTab implementation. */
    @Override
    public View getView() {
        return mShareView.getView();
    }

    @Override
    public void onResume() {
        mMediator.setIsOnForeground(true);
        RecordUserAction.record("SharingQRCode.TabVisible.Share");
    }

    @Override
    public void onPause() {
        mMediator.setIsOnForeground(false);
    }

    @Override
    public void onDestroy() {}

    @Override
    public void updatePermissions(WindowAndroid windowAndroid) {
        if (mMediator != null) {
            mMediator.updatePermissions(windowAndroid);
        }
    }
}
