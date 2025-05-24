// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;

/** Helper class to track creating new tabs with a NewTabPage. */
public class NewTabPageCreationTracker {
    private final TabModelSelector mTabModelSelector;
    private TabCreationRecorder mTabCreationRecorder;

    /**
     * Constructor.
     *
     * @param tabModelSelector Tab model selector to observe tab creation event.
     */
    public NewTabPageCreationTracker(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    /** Starts tracking new NTPs opened in a new tab. */
    public void monitorNtpCreation() {
        mTabCreationRecorder = new TabCreationRecorder();
        mTabModelSelector.addObserver(mTabCreationRecorder);
    }

    /**
     * Tracks new NTPs opened in a new tab. Use through {@link
     * NewTabPageCreationTracker#monitorNtpCreation(TabModelSelector)}.
     */
    private static class TabCreationRecorder implements TabModelSelectorObserver {
        @Override
        public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
            if (tab.isOffTheRecord() || !UrlUtilities.isNtpUrl(tab.getUrl())) {
                return;
            }

            RecordHistogram.recordEnumeratedHistogram(
                    "NewTabPage.OpenedInNewTab", tab.getLaunchType(), TabLaunchType.SIZE);

            if (tab.getLaunchType() == TabLaunchType.FROM_CHROME_UI
                    || tab.getLaunchType() == TabLaunchType.FROM_TAB_GROUP_UI
                    || tab.getLaunchType() == TabLaunchType.FROM_TAB_SWITCHER_UI) {
                var state = NewTabPageCreationState.from(tab);
                if (state != null) state.onNewTabCreated();
            }
        }
    }

    /** Destroy and unhook objects at destruction. */
    public void destroy() {
        if (mTabCreationRecorder != null) mTabModelSelector.removeObserver(mTabCreationRecorder);
    }
}
