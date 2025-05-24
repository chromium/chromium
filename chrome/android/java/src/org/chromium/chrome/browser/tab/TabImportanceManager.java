// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.content_public.browser.ChildProcessImportance;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * Manages the importance for all Tabs in the same process. Ensures that at least one tab is
 * important, and unless multiple tabs are simultaneously visible, only one is important.
 */
@NullMarked
public class TabImportanceManager {
    // Typically no more than 2 visible tabs at once (multi-window).
    private static final List<Tab> sImportantTabs = new ArrayList<>(2);

    public static void tabShown(Tab shownTab) {
        ThreadUtils.assertOnUiThread();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PROCESS_RANK_POLICY_ANDROID)) {
            return;
        }
        boolean isImportant =
                ChromeFeatureList.isEnabled(ChromeFeatureList.CHANGE_UNFOCUSED_PRIORITY);
        setImportance(
                shownTab,
                isImportant ? ChildProcessImportance.IMPORTANT : ChildProcessImportance.MODERATE);
        // Shown tabs should always be important, but hidden tabs should only be normal if there's
        // at least one important tab for two reasons:
        // 1. We could be switching between tabs within the same process and don't want the process
        //      to be killed while switching.
        // 2. We want the most recently used tab to stay alive.
        Iterator<Tab> it = sImportantTabs.iterator();
        while (it.hasNext()) {
            TabImpl importantTab = (TabImpl) it.next();
            if (importantTab.isHidden()) {
                importantTab.setImportance(ChildProcessImportance.NORMAL);
                it.remove();
            }
        }
        if (!sImportantTabs.contains(shownTab)) sImportantTabs.add(shownTab);
    }

    public static void tabDestroyed(Tab tab) {
        ThreadUtils.assertOnUiThread();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PROCESS_RANK_POLICY_ANDROID)) {
            return;
        }
        sImportantTabs.remove(tab);
    }

    public static void setImportance(Tab tab, @ChildProcessImportance int importance) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PROCESS_RANK_POLICY_ANDROID)) {
            return;
        }
        ((TabImpl) tab).setImportance(importance);
    }
}
