// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.findinpage;

/**
 * Observer for find in page actions.
 */
public interface FindToolbarObserver {
    /**
     * Notified when the find in page toolbar has been shown.
     */
    void onFindToolbarShown();

    /**
     * Notified when the find in page toolbar has been hidden.
     */
    void onFindToolbarHidden();
}
