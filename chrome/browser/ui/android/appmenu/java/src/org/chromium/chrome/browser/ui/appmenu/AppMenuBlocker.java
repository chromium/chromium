// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import org.chromium.build.annotations.NullMarked;

/**
 * An interface that may be used to block the app menu from showing (e.g. when other conflicting UI
 * is showing). To register, see {@link AppMenuCoordinator#registerAppMenuBlocker(AppMenuBlocker)}.
 */
@NullMarked
public interface AppMenuBlocker {
    /**
     * @return Whether the app menu can be shown.
     */
    boolean canShowAppMenu();
}
