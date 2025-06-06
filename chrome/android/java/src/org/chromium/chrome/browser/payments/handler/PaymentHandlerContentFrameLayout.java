// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.payments.ui.InputProtector;

/** A FrameLayout implementation used to ignore user input based on an InputProtector. */
@NullMarked
public class PaymentHandlerContentFrameLayout extends FrameLayout {
    private @Nullable InputProtector mInputProtector;

    /**
     * Constructor for inflation from XML.
     *
     * @param context The Android context.
     * @param atts The XML attributes.
     */
    public PaymentHandlerContentFrameLayout(Context context, AttributeSet atts) {
        super(context, atts);
    }

    /** Set the input protector to be used for touch interception. */
    public void setInputProtector(InputProtector inputProtector) {
        mInputProtector = inputProtector;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        assert mInputProtector != null;
        return !mInputProtector.shouldInputBeProcessed();
    }
}
