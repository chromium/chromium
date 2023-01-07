// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

/**
 * Provides access to utilities used for feed service.
 */
public interface FeedServiceUtil {
    /** Returns the enabled state of tab group feature. */
    @TabGroupEnabledState
    int getTabGroupEnabledState();
}
