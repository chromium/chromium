// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.scroll_capture;

import android.os.Build.VERSION_CODES;
import android.view.ScrollCaptureCallback;
import android.view.View;

import androidx.annotation.RequiresApi;

import org.chromium.chrome.browser.share.scroll_capture.ScrollCaptureCallbackDelegate.EntryManagerWrapper;
import org.chromium.chrome.browser.tab.Tab;

/** Delegate to handle Android S API calls for {@link ScrollCaptureManager}. */
@RequiresApi(api = VERSION_CODES.S)
public class ScrollCaptureManagerDelegateImpl implements ScrollCaptureManagerDelegate {
    private final ScrollCaptureCallbackImpl mScrollCaptureCallback;

    ScrollCaptureManagerDelegateImpl() {
        mScrollCaptureCallback = new ScrollCaptureCallbackImpl(new EntryManagerWrapper());
    }

    @Override
    public void addScrollCaptureBindings(View view) {
        setScrollCaptureCallbackForView(view, mScrollCaptureCallback);
    }

    @Override
    public void removeScrollCaptureBindings(View view) {
        setScrollCaptureCallbackForView(view, null);
    }

    @Override
    public void setCurrentTab(Tab tab) {
        mScrollCaptureCallback.setCurrentTab(tab);
    }

    private void setScrollCaptureCallbackForView(
            View view, ScrollCaptureCallback scrollCaptureCallback) {
        view.setScrollCaptureHint(
                scrollCaptureCallback != null
                        ? View.SCROLL_CAPTURE_HINT_INCLUDE
                        : View.SCROLL_CAPTURE_HINT_AUTO);
        view.setScrollCaptureCallback(scrollCaptureCallback);
    }
}
