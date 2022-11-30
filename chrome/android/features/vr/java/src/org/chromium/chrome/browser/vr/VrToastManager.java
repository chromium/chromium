// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

/**
 * Interface that is needed to manage a VR toast.
 */
public interface VrToastManager {
    /**
     * Show a Toast (contains only text) in VR.
     */
    void showToast(CharSequence text);

    /**
     * Cancel a Toast (contains only text) in VR.
     */
    void cancelToast();
}
