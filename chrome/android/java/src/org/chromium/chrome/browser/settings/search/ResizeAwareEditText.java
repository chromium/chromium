// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.EditText;

import androidx.appcompat.widget.AppCompatEditText;

import org.chromium.build.annotations.NullMarked;

/**
 * An {@link EditText} that keeps its horizontal scroll offset in sync with its width even when the
 * width changes without a layout pass.
 *
 * <p>A single-line {@link android.widget.TextView} lays its text (and hint) out in a very wide
 * layout and positions it by scrolling. For RTL text, the scroll offset keeps the text flush with
 * the right edge and so depends on the view's width. TextView only recomputes that offset after a
 * measure pass. A {@link android.transition.ChangeBounds} transition, however, resizes the view via
 * {@link android.view.View#setLeftTopRightBottom} without measuring, which leaves RTL text drawn at
 * the offset for the old width: shifted sideways and clipped. See https://crbug.com/568207264.
 */
@NullMarked
public class ResizeAwareEditText extends AppCompatEditText {
    public ResizeAwareEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        if (w == oldw) return;

        // Mirrors what TextView does on its own pre-draw pass after a measure.
        int selectionEnd = getSelectionEnd();
        if (selectionEnd >= 0) bringPointIntoView(selectionEnd);
    }
}
