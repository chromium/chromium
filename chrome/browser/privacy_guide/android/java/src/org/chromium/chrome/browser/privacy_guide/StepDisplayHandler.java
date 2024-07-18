// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

/** Utilities to determine whether a privacy guide step will be displayed or not. */
public interface StepDisplayHandler {
    /**
     * @return Whether the Privacy Guide History Sync page should be displayed.
     */
    boolean shouldDisplayHistorySync();

    /**
     * @return Whether the Privacy Guide Safe Browsing page should be displayed.
     */
    boolean shouldDisplaySafeBrowsing();

    /**
     * @return Whether the Privacy Guide Cookies page should be displayed.
     */
    boolean shouldDisplayCookies();

    /**
     * @return Whether the Privacy Guide Preload page should be displayed.
     */
    boolean shouldDisplayPreload();

    /**
     * @return Whether the Privacy Guide Ad Topics page should be displayed.
     */
    boolean shouldDisplayAdTopics();
}
