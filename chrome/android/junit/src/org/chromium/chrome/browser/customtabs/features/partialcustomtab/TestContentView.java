// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import android.content.Context;
import android.view.MotionEvent;

import org.chromium.components.embedder_support.view.ContentView;

import java.util.ArrayList;
import java.util.List;

/** Records touch events, which a ContentView without WebContents ignores. */
class TestContentView extends ContentView {
    private final List<MotionEvent> mTouchEvents = new ArrayList<>();

    TestContentView(Context context) {
        super(context, /* webContents= */ null);
    }

    List<MotionEvent> getTouchEvents() {
        return mTouchEvents;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        mTouchEvents.add(event);
        return super.onTouchEvent(event);
    }
}
