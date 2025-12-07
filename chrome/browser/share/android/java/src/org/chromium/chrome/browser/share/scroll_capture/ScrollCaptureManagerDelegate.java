// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.scroll_capture;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;

/** Delegate to handle Android S API calls for {@link ScrollCaptureManager}. */
@NullMarked
public interface ScrollCaptureManagerDelegate {
    /** Sets up scroll capture API for a {@link View}. */
    void addScrollCaptureBindings(View view);

    /** Removes the scroll capture API bindings from a {@link View}. */
    void removeScrollCaptureBindings(View view);

    /** Updates the current tab. */
    void setCurrentTab(Tab tab);
}
