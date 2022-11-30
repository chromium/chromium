// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

/**
 * This class implements a Toast which is similar to Android Toast in VR.
 */
public class VrToast extends android.widget.Toast {
    private VrToastManager mVrToastManager;

    public VrToast(Context context, VrToastManager vrToastManager) {
        super(context);
        mVrToastManager = vrToastManager;
    }

    /**
     * In VR, only the text of the first TextView in the Toast's view hierarchy is displayed. While
     * this is not perfect, but all the Toasts in Android only has one TextView currently. So it
     * works.
     * TODO(bshe): we should either enforce one TextView for Toast or update this function to handle
     * more general cases.
     */
    @Override
    public void show() {
        TextView textView = findTextViewRecursive(getView());
        assert textView != null;
        mVrToastManager.showToast(textView.getText());
    }

    @Override
    public void cancel() {
        mVrToastManager.cancelToast();
    }

    private TextView findTextViewRecursive(View view) {
        if (view instanceof TextView) return (TextView) view;
        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                View child = viewGroup.getChildAt(i);
                TextView textView = findTextViewRecursive(child);
                if (textView != null) return textView;
            }
        }
        return null;
    }
}
