// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.TextView;

/**
 * Text view that calculates and sets maximum number of visible lines. Max lines is necessary for
 * ellipsis to work.
 */
public class LineLimitedTextView extends TextView {
    private boolean mIsEllipsized;
    private Runnable mIsEllipsizedListener;

    /** Constructor for inflating from XML. */
    public LineLimitedTextView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public boolean onPreDraw() {
        int maxLines = (getHeight() - getPaddingTop() - getPaddingBottom()) / getLineHeight();
        setMaxLines(maxLines);

        if (!mIsEllipsized && getLayout().getLineCount() > maxLines) {
            mIsEllipsized = true;
            notifyIsEllipsized();
        }
        return true;
    }

    public void setIsEllipsizedListener(Runnable isEllipsizedListener) {
        mIsEllipsizedListener = isEllipsizedListener;
    }

    private void notifyIsEllipsized() {
        if (mIsEllipsizedListener != null) mIsEllipsizedListener.run();
    }
}
