// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;

import java.util.List;

/** An interface for a RestoreTabsController instance. */
@NullMarked
public interface RestoreTabsController {
    /** Destroy when lifecycle of the controller ends. */
    void destroy();

    /** Show the home screen of the restore tabs promo when triggered. */
    void showHomeScreen(
            ForeignSessionHelper foreignSessionHelper,
            List<ForeignSession> sessions,
            RestoreTabsControllerDelegate delegate);
}
