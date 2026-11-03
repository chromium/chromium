// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabLaunchTypeUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;

/** Helper class to track creating new tabs with a NewTabPage. */
@NullMarked
public class NewTabPageCreationTracker {
    private final TabModelSelector mTabModelSelector;
    private @Nullable TabCreationRecorder mTabCreationRecorder;

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
        mTabCreationRecorder = new TabCreationRecorder(mTabModelSelector);
    }

    /**
     * Tracks new NTPs opened in a new tab. Use through {@link
     * NewTabPageCreationTracker#monitorNtpCreation(TabModelSelector)}.
     */
    private static class TabCreationRecorder extends TabModelSelectorTabModelObserver {
        TabCreationRecorder(TabModelSelector tabModelSelector) {
            super(tabModelSelector);
        }

        @Override
        public void didAddTab(
                Tab tab,
                @TabLaunchType int type,
                @TabCreationState int creationState,
                boolean markedForSelection) {
            if (tab.isOffTheRecord() || !UrlUtilities.isNtpUrl(tab.getUrl())) {
                return;
            }

            RecordHistogram.recordEnumeratedHistogram(
                    "NewTabPage.OpenedInNewTab", tab.getLaunchType(), TabLaunchType.SIZE);

            if (TabLaunchTypeUtils.isExplicitUserNtpLaunch(tab.getLaunchType())) {
                var state = NewTabPageCreationState.from(tab);
                if (state != null) state.didAddTab();
            }
        }
    }

    /** Destroy and unhook objects at destruction. */
    public void destroy() {
        if (mTabCreationRecorder != null) mTabCreationRecorder.destroy();
    }
}
