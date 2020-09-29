// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

/** Common interface for the bottom bar delegate. */
public interface AssistantBottomBarDelegate {
    /** The back button has been pressed. */
    boolean onBackButtonPressed();

    // TODO(micantox): Instead of a dedicated notification, we should just notify the controller of
    // the new bottom sheet state and have the logic to shutdown there. Currently, this would be
    // tricky to do because it would interfere with the existing Controller::SetBottomSheetState
    // method and in particular tab switching.
    /**
     * The bottom sheet was closed with a swipe gesture. Note that this will be fired both when
     * going into the PEEK state as well as when dismissing the sheet altogether.
     */
    void onBottomSheetClosedWithSwipe();
}
