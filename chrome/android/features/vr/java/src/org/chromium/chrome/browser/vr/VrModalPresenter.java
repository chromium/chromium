// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;
import org.chromium.chrome.browser.modaldialog.ModalDialogViewBinder;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The presenter that shows a {@link ModalDialogView} in an Android dialog. */
public class VrModalPresenter extends ModalDialogManager.Presenter {
    private final Context mContext;
    private final VrDialogManager mVrDialogManager;

    private VrDialog mVrDialog;
    private PropertyModelChangeProcessor<PropertyModel, ModalDialogView, PropertyKey>
            mModelChangeProcessor;

    public VrModalPresenter(Context context, VrDialogManager vrDialogManager) {
        mContext = context;
        mVrDialogManager = vrDialogManager;
    }

    @Override
    protected void addDialogView(PropertyModel model) {
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                MarginLayoutParams.MATCH_PARENT, MarginLayoutParams.WRAP_CONTENT, Gravity.CENTER);

        mVrDialog = new VrDialog(mContext, mVrDialogManager);
        int style = model.get(ModalDialogProperties.PRIMARY_BUTTON_FILLED)
                ? R.style.Theme_Chromium_ModalDialog_FilledPrimaryButton
                : R.style.Theme_Chromium_ModalDialog_TextPrimaryButton;
        ModalDialogView dialogView =
                (ModalDialogView) LayoutInflater.from(new ContextThemeWrapper(mContext, style))
                        .inflate(R.layout.modal_dialog_view, null);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(model, dialogView, new ModalDialogViewBinder());
        mVrDialog.addView(dialogView, params);
        mVrDialogManager.setDialogView(mVrDialog);
        mVrDialog.initVrDialog();
    }

    @Override
    protected void removeDialogView(PropertyModel model) {
        // Dismiss the currently showing dialog.
        if (mVrDialog != null) mVrDialog.dismiss();
        mVrDialogManager.setDialogView(null);
        mVrDialog = null;

        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
            mModelChangeProcessor = null;
        }
    }

    public void closeCurrentDialog() {
        dismissCurrentDialog(DialogDismissalCause.UNKNOWN);
    }
}
