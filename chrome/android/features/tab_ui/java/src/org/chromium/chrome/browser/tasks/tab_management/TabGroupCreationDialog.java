// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

/** Interface for the tab group creation dialog related UI. */
public interface TabGroupCreationDialog {
    /** Remove all tab group model observers. */
    void destroy();

    /** Add observers to notify when a new tab group is being created. */
    void addObservers();

    /** Remove observers monitoring tab group creation that display a custom dialog. */
    void removeObservers();
}
