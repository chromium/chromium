// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

/** Package private interface extension of {@link TabGroupModelFilter} */
// TODO(crbug.com/372068933): Actually make this package private, currently public so that the
// tab_groups/ module doesn't need to all get moved into chrome/android/.
public interface TabGroupModelFilterInternal extends TabGroupModelFilter {
    /**
     * Mark TabState initialized, and TabGroupModelFilter ready to use. This should only be called
     * once, and should only be called by {@link TabGroupModelFilterProvider}.
     */
    void markTabStateInitialized();
}
