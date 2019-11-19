// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;

/**
 * Interface used by ChromeActivity to communicate with AR code that is only
 * available if |enable_arcore| is set to true at build time.
 */
public interface ArDelegate {
    /**
     * Needs to be called once native libraries are available.
     **/
    public void init();

    /**
     * Needs to be called in Activity's onResumeWithNative() method in order
     * to notify AR that the activity was resumed.
     **/
    public void registerOnResumeActivity(Activity activity);

    /**
     * Used to let AR immersive mode intercept the Back button to exit immersive mode.
     */
    public boolean onBackPressed();
}
