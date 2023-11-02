// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Color;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

/**
 * Manages how dialogs look inside of VR.
 */
public class VrDialog extends FrameLayout {
    private static final int DIALOG_WIDTH = 1200;
    private VrDialogManager mVrDialogManager;

    /**
     * Constructor of VrDialog. Sets the DialogManager that will be used to
     * communicate with the vr presentation of the dialog.
     */
    // For some reason we have to use Gravity.LEFT instead of Gravity.{START|END}. This works for
    // both LTR and RTL languages.
    @SuppressLint("RtlHardcoded")
    public VrDialog(Context context, VrDialogManager vrDialogManager) {
        super(context);
        setLayoutParams(new FrameLayout.LayoutParams(
                DIALOG_WIDTH, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.LEFT));
        setBackgroundColor(Color.WHITE);
        mVrDialogManager = vrDialogManager;
        mVrDialogManager.setDialogFloating(false);
    }

    /**
     * Dismiss whatever dialog that is shown in VR.
     */
    public void dismiss() {
        mVrDialogManager.closeVrDialog();
    }

    /**
     * Initialize a dialog in VR based on the layout that was set by {@link
     * #setLayout(FrameLayout)}. This also adds a OnLayoutChangeListener to make sure that Dialog in
     * VR has the correct size.
     */
    public void initVrDialog() {
        addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                mVrDialogManager.setDialogSize(getWidth(), getHeight());
            }
        });
        // TODO(asimjour): remove this when Keyboard supports native ui.
        disableSoftKeyboard(this);
        mVrDialogManager.initVrDialog(getWidth(), getHeight());
    }

    private void disableSoftKeyboard(ViewGroup viewGroup) {
        for (int i = 0; i < viewGroup.getChildCount(); i++) {
            View view = viewGroup.getChildAt(i);
            if (view instanceof ViewGroup) {
                disableSoftKeyboard((ViewGroup) view);
            } else if (view instanceof TextView) {
                TextView text = (TextView) view;
                // It is important to avoid setting InputType to NULL again. Otherwise, it will
                // change the TextView to single line mode and cause other unexpected issues(such as
                // text in button is not captiablized).
                int type = text.getInputType();
                if (type != InputType.TYPE_NULL) {
                    text.setInputType(InputType.TYPE_NULL);
                    // If the TextView has multi line flag, reset line mode to multi line.
                    if ((type & (InputType.TYPE_MASK_CLASS | InputType.TYPE_TEXT_FLAG_MULTI_LINE))
                            == (InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE)) {
                        text.setSingleLine(false);
                    }
                }
            }
        }
    }
}
