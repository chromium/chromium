// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.util.AttributeSet;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.FrameLayout;

/** Basic functionality to needed for FrameLayouts used as the root view of NativePages. */
public class NativePageRootFrameLayout extends FrameLayout {
    /** Constructor for inflating from XML. */
    public NativePageRootFrameLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        // Fixes lanscape transitions when unfocusing the URL bar: crbug.com/288546
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN;
        return super.onCreateInputConnection(outAttrs);
    }
}
