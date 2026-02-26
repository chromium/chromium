// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Utility methods for interacting between ActorService and Tabs. */
@NullMarked
public class ActorServiceTabUtils {
    /**
     * Returns a list of task IDs that are actuating in the given list of tabs.
     *
     * @param tabModel The {@link TabModel} to act on.
     * @param tabs The list of {@link Tab}s to check for active tasks.
     * @return A list of active task IDs.
     */
    public static List<@ActorTaskId Integer> getOnGoingActorTasks(
            @Nullable TabModel tabModel, @Nullable List<Tab> tabs) {
        if (tabModel == null || tabs == null || tabs.isEmpty()) return Collections.emptyList();
        Profile profile = tabModel.getProfile();
        if (profile == null) return Collections.emptyList();

        ActorKeyedService service = ActorKeyedServiceFactory.getForProfile(profile);
        if (service == null) return Collections.emptyList();

        Set<@ActorTaskId Integer> taskIds = new HashSet<>();
        for (Tab tab : tabs) {
            @Nullable
            @ActorTaskId
            Integer taskId = service.getActiveTaskIdOnTab(tab.getId());
            if (taskId != null) {
                taskIds.add(taskId);
            }
        }
        return new ArrayList<>(taskIds);
    }
}
