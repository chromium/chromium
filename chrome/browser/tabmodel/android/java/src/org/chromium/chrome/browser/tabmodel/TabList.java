// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.MockedInTests;
import org.chromium.chrome.browser.tab.Tab;

/**
 * A read only list of {@link Tab}s. This list understands the concept of an incognito list as well
 * as a currently selected tab (see {@link #index}).
 */
@MockedInTests
public interface TabList {
    // Keep this in sync with chrome/browser/ui/android/tab_model/tab_model.cc
    int INVALID_TAB_INDEX = -1;

    /**
     * TODO(crbug.com/350654700): clean up usages and remove isIncognito.
     *
     * <p>Returns whether this tab model contains only incognito tabs or only normal tabs.
     *
     * @deprecated Use {@link #isIncognitoBranded()} or {@link #isOffTheRecord()}.
     */
    @Deprecated
    boolean isIncognito();

    /**
     * Returns whether this tab model contains only off-the-record tabs or only normal tabs.
     *
     * @see {@link Profile#isOffTheRecord()}
     */
    boolean isOffTheRecord();

    /**
     * Returns whether this tab model contains only incognito branded tabs or only normal tabs.
     *
     * @see {@link Profile#isIncognitoBranded()}
     */
    boolean isIncognitoBranded();

    /** Returns the index of the current tab, or {@link #INVALID_TAB_INDEX} if there are no tabs. */
    int index();

    /** Returns the number of open tabs in this model. */
    int getCount();

    /**
     * Get the tab at the specified position
     *
     * @param index The index of the {@link Tab} to return.
     * @return The {@code Tab} at position {@code index}, or {@code null} if {@code index} < 0 or
     *     {@code index} >= {@link #getCount()}.
     */
    Tab getTabAt(int index);

    /** Returns the index of the given tab in the order of the tab stack. */
    int indexOf(Tab tab);
}
