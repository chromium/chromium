// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import androidx.annotation.NonNull;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.Tab;

import java.util.HashSet;

import javax.inject.Inject;

/**
 * Class used to count the number of tabs opened in a custom tab session. Used to understand how
 * often a user navigates to links with "target=_blank" in one single CCT session.
 */
@ActivityScope
public class CustomTabCountObserver extends CustomTabActivityTabProvider.Observer {
    private final HashSet<Integer> mSeenTabId = new HashSet<>();

    @Inject
    public CustomTabCountObserver(CustomTabActivityTabProvider tabProvider) {
        tabProvider.addObserver(this);
        if (tabProvider.getTab() != null) {
            recordTabSeen(tabProvider.getTab());
        }
    }

    @Override
    public void onInitialTabCreated(@NonNull Tab tab, int mode) {
        recordTabSeen(tab);
    }

    @Override
    public void onTabSwapped(@NonNull Tab tab) {
        recordTabSeen(tab);
    }

    @Override
    public void onAllTabsClosed() {
        mSeenTabId.clear();
    }

    private void recordTabSeen(@NonNull Tab tab) {
        if (mSeenTabId.add(tab.getId())) {
            RecordHistogram.recordCount100Histogram(
                    "CustomTabs.TabCounts.UniqueTabsSeen", mSeenTabId.size());
        }
    }
}
