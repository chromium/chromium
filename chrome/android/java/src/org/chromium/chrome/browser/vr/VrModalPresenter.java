// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.modaldialog.DialogDismissalCause;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager;

/** The presenter that shows a {@link ModalDialogView} in an Android dialog. */
public class VrModalPresenter extends ModalDialogManager.Presenter {
    private VrDialog mVrDialog;
    private VrDialogManager mVrDialogManager;

    public VrModalPresenter(VrDialogManager vrDialogManager) {
        mVrDialogManager = vrDialogManager;
    }

    @Override
    protected void addDialogView(View dialogView) {
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                MarginLayoutParams.MATCH_PARENT, MarginLayoutParams.WRAP_CONTENT, Gravity.CENTER);

        mVrDialog = new VrDialog(dialogView.getContext(), mVrDialogManager);
        mVrDialog.addView(dialogView, params);
        mVrDialogManager.setDialogView(mVrDialog);
        mVrDialog.initVrDialog();
    }

    @Override
    protected void removeDialogView(View dialogView) {
        // Dismiss the currently showing dialog.
        if (mVrDialog != null) {
            mVrDialog.dismiss();
        }
        mVrDialogManager.setDialogView(null);
        mVrDialog = null;
    }

    public void closeCurrentDialog() {
        dismissCurrentDialog(DialogDismissalCause.UNKNOWN);
    }
}
