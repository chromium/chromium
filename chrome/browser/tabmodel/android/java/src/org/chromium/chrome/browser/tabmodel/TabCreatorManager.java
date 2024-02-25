// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

/** An interface to return a {@link TabCreator} either for regular or incognito tabs. */
public interface TabCreatorManager {
    /**
     * @return A {@link TabCreator} that will create either regular or incognito tabs.
     * @param incognito True if the method should return the TabCreator for incognito tabs, false
     *                  for regular tabs.
     */
    TabCreator getTabCreator(boolean incognito);
}
