// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.infobar;

import org.chromium.build.annotations.NullMarked;

/** A listener to interact with the different subpanels of the translate infobar. */
@NullMarked
public interface SubPanelListener {

    /**
     * Called whenever a sub panel is closed.
     *
     * @param action one of the action types in {@code InfoBar}
     */
    void onPanelClosed(int action);

    /** Called to indicate that the current options should be persisted. */
    void onOptionsChanged();
}
