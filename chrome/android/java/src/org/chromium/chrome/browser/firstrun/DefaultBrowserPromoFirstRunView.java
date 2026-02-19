// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RelativeLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.ui.widget.ButtonCompat;

/** View for the Default Browser Promo during the First Run Experience. */
@NullMarked
public class DefaultBrowserPromoFirstRunView extends RelativeLayout {
    private ButtonCompat mContinueButton;
    private ButtonCompat mDismissButton;

    public DefaultBrowserPromoFirstRunView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mContinueButton = findViewById(R.id.fre_continue_button);
        mDismissButton = findViewById(R.id.fre_dismiss_button);
    }

    public ButtonCompat getContinueButtonView() {
        assert mContinueButton != null;
        return mContinueButton;
    }

    public ButtonCompat getDismissButtonView() {
        assert mDismissButton != null;
        return mDismissButton;
    }
}
