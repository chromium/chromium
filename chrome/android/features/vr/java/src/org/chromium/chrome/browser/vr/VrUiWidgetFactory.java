// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.content.Context;
import android.widget.PopupWindow;

import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.UiWidgetFactory;

/**
 * The factory that creates VR compatible UI widgets.
 */
public class VrUiWidgetFactory extends UiWidgetFactory {
    private VrShell mVrShell;
    private ModalDialogManager mModalDialogManager;

    public VrUiWidgetFactory(VrShell vrShell, ModalDialogManager modalDialogManager) {
        mVrShell = vrShell;
        mModalDialogManager = modalDialogManager;
    }

    @Override
    public PopupWindow createPopupWindow(Context context) {
        return new VrPopupWindow(context, mVrShell);
    }

    @Override
    public android.widget.Toast createToast(Context context) {
        return new VrToast(context, mVrShell);
    }

    @Override
    public AlertDialog createAlertDialog(Context context) {
        return new VrAlertDialog(context, mModalDialogManager);
    }

    @Override
    @SuppressLint("ShowToast")
    public android.widget.Toast makeToast(Context context, CharSequence text, int duration) {
        android.widget.Toast toast = new VrToast(context, mVrShell);
        // It is tempting to use toast.setText directly instead of creating a tmpToast and
        // calling setView below. However, setText depends on android.widget.Toast.makeText
        // being called first to inflate a subtree which has a Textview. Calling setText without
        // a Textview will result an exception.
        android.widget.Toast tmpToast = android.widget.Toast.makeText(context, text, duration);
        toast.setView(tmpToast.getView());
        toast.setDuration(tmpToast.getDuration());
        return toast;
    }
}
