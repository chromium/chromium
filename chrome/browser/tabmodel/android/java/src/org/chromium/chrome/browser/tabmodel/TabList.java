// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.MockedInTests;
import org.chromium.chrome.browser.tab.Tab;

/**
 * A read only list of {@link Tab}s. This list understands the concept of an incognito list as
 * well as a currently selected tab (see {@link #index}).
 */
@MockedInTests
public interface TabList {
    // Keep this in sync with chrome/browser/ui/android/tab_model/tab_model.cc
    public static final int INVALID_TAB_INDEX = -1;

    /**
     * @return Whether this tab model contains only incognito tabs or only normal tabs.
     */
    boolean isIncognito();

    /**
     * @return The index of the current tab, or {@link #INVALID_TAB_INDEX} if there are no tabs.
     */
    int index();

    /**
     * @return the number of open tabs in this model
     */
    int getCount();

    /**
     * Get the tab at the specified position
     *
     * @param index The index of the {@link Tab} to return.
     * @return The {@code Tab} at position {@code index}, or {@code null} if {@code index} < 0
     *         or {@code index} >= {@link #getCount()}.
     */
    Tab getTabAt(int index);

    /**
     * @return index of the given tab in the order of the tab stack.
     */
    int indexOf(Tab tab);
}
