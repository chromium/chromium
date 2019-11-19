// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.addtohomescreen;

/**
 * Used by {@link AddToHomescreenDialogView} to propagate view events to the business logic.
 */
public interface AddToHomescreenViewDelegate {
    /**
     * Called when the user accepts adding the item to the home screen with the provided title.
     */
    void onAddToHomescreen(String title);

    /**
     * Called when the user requests app details.
     * @return Whether the view should be dismissed.
     */
    boolean onAppDetailsRequested();

    /**
     * Called when the view's lifetime is over and it disappears from the screen.
     */
    void onViewDismissed();
}
