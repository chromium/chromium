// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.scan_tab;

import android.content.Context;
import android.view.View;

import org.chromium.base.BuildInfo;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.share.qrcode.QrCodeDialogTab;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Creates and represents the QrCode scan panel UI.
 */
public class QrCodeScanCoordinator implements QrCodeDialogTab {
    private final QrCodeScanView mScanView;
    private final QrCodeScanMediator mMediator;

    /**
     * The QrCodeScanCoordinator constructor.
     *
     * @param context The context to use for user permissions.
     * @param observer The observer for navigation event.
     * @param windowAndroid The {@link WindowAndroid} for the containing activity.
     */
    public QrCodeScanCoordinator(Context context, QrCodeScanMediator.NavigationObserver observer,
            WindowAndroid windowAndroid) {
        PropertyModel scanViewModel = new PropertyModel(QrCodeScanViewProperties.ALL_KEYS);
        mMediator = new QrCodeScanMediator(context, scanViewModel, observer, windowAndroid);

        mScanView = new QrCodeScanView(
                context, mMediator::onPreviewFrame, mMediator::promptForCameraPermission);
        PropertyModelChangeProcessor.create(scanViewModel, mScanView, new QrCodeScanViewBinder());
    }

    /** QrCodeDialogTab implementation. */
    @Override
    public View getView() {
        return mScanView.getView();
    }

    @Override
    public boolean isEnabled() {
        return !BuildInfo.getInstance().isAutomotive;
    }

    @Override
    public void onResume() {
        RecordUserAction.record("SharingQRCode.TabVisible.Scan");
        mMediator.setIsOnForeground(true);
    }

    @Override
    public void onPause() {
        mMediator.setIsOnForeground(false);
    }

    @Override
    public void onDestroy() {
        mScanView.stopCamera();
    }
    @Override
    public void updatePermissions(WindowAndroid windowAndroid) {}
}
