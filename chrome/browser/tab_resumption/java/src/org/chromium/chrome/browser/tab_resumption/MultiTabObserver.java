// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;

import java.util.HashMap;
import java.util.Map;

/** A observer for multiple Tabs that performs the same action for each tab. */
public class MultiTabObserver extends EmptyTabObserver {
    private Map<Integer, Tab> mObservedTabMap;

    MultiTabObserver() {
        mObservedTabMap = new HashMap<Integer, Tab>();
    }

    /** Must be called by the owner to clean up. */
    public void destroy() {
        clear();
    }

    /** Starts to observe a given Tab. */
    public void add(Tab tab) {
        if (!mObservedTabMap.containsKey(tab.getId())) {
            tab.addObserver(this);
            mObservedTabMap.put(tab.getId(), tab);
        }
    }

    /** Stops observing all currently observed Tabs. */
    public void clear() {
        for (Map.Entry<Integer, Tab> mapEntry : mObservedTabMap.entrySet()) {
            mapEntry.getValue().removeObserver(this);
        }
        mObservedTabMap.clear();
    }
}
