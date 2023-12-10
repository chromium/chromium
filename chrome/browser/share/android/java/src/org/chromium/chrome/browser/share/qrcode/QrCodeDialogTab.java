// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import android.view.View;

import org.chromium.ui.base.WindowAndroid;

/** Common interface for all the tab components in QrCodeDialog. */
public interface QrCodeDialogTab {
    public View getView();

    /**
     * @return whether the tab is currently enabled.
     */
    default boolean isEnabled() {
        return true;
    }

    /** Called when the entire dialog is resumed. */
    public void onResume();

    /** Called when the entire dialog is paused. */
    public void onPause();

    /**
     *  Called when the dialog is destroyed. This happens when the user has navigated away from the
     *  dialog.
     */
    public void onDestroy();

    /** Called when the permissions delegate is reset. */
    public void updatePermissions(WindowAndroid windowAndroid);
}
