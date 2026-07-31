// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Manages tab detachment and transitions from physical activities to offscreen background
 * rendering.
 */
@NullMarked
public class ActorTabStateHelper {

    private ActorTabStateHelper() {}

    /**
     * Identifies, detaches, and returns tabs running active tasks from the given selector as
     * background sessions.
     *
     * @param selector The TabModelSelector of the stopping activity.
     * @return A list of prepared BackgroundSession objects.
     */
    public static List<BackgroundSession> detachActiveBackgroundSessions(
            TabModelSelector selector) {
        ThreadUtils.assertOnUiThread();
        TabModel regularModel = selector.getModel(/* incognito= */ false);
        ActorKeyedService service = getActorKeyedService(regularModel);

        if (regularModel == null || service == null) {
            return Collections.emptyList();
        }

        List<BackgroundSession> sessionsToTransition = findActiveSessions(regularModel, service);

        // Detach active task tabs from the UI model.
        // TODO(crbug.com/537330680): Handle edge case when a tab group only has one tab.
        // TODO(crbug.com/540427987): Handle edge case when restoring a previously pinned tab.
        for (BackgroundSession session : sessionsToTransition) {
            regularModel
                    .getTabRemover()
                    .removeTab(session.getLastActiveTab(), /* allowDialog= */ false);
        }

        return sessionsToTransition;
    }

    private static @Nullable ActorKeyedService getActorKeyedService(@Nullable TabModel model) {
        if (model == null) return null;
        Profile profile = model.getProfile();
        if (profile == null) return null;
        return ActorKeyedServiceFactory.getForProfile(profile.getOriginalProfile());
    }

    private static List<BackgroundSession> findActiveSessions(
            TabModel model, ActorKeyedService service) {
        if (service.getActiveTasksCount() == 0) {
            return Collections.emptyList();
        }
        List<BackgroundSession> sessions = new ArrayList<>();
        for (Tab tab : model) {
            if (tab == null) continue;
            Integer taskId = ActorTaskHelper.getActiveTaskIdOnTab(service, tab);
            if (taskId != null) {
                BackgroundSession session = getSessionForTask(sessions, taskId);
                if (session != null) {
                    session.addTab(tab);
                } else {
                    sessions.add(new BackgroundSession(tab, taskId));
                }
            }
        }
        return sessions;
    }

    private static @Nullable BackgroundSession getSessionForTask(
            List<BackgroundSession> sessions, int taskId) {
        for (BackgroundSession session : sessions) {
            if (session.getTaskId() != null && session.getTaskId() == taskId) {
                return session;
            }
        }
        return null;
    }
}
