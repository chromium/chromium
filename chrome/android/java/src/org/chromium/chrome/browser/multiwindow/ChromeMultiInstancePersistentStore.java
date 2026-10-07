// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.INVALID_WINDOW_ID;

import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceDataProto.InstanceData;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.SessionStartupPolicy;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Manages persisted instance state. This includes information pertinent to an instance that may be
 * active (ie. a Chrome window associated with a live activity / task), inactive, or recently closed
 * by the user.
 */
@NullMarked
class ChromeMultiInstancePersistentStore extends MultiInstancePersistentStore {
    private static class InstanceDataWithId {
        private final int mId;
        private final InstanceData mInstanceData;

        private InstanceDataWithId(int id, InstanceData instanceData) {
            mId = id;
            mInstanceData = instanceData;
        }
    }

    static Set<Integer> readAllInstanceIds() {
        assert sData != null;
        return sData.getInstancesMap().keySet();
    }

    static boolean hasInstance(int instanceId) {
        return readLastAccessedTime(instanceId) != 0;
    }

    static void deleteInstanceState(int instanceId) {
        assert sData != null;
        sData = sData.toBuilder().removeInstances(instanceId).build();
        saveProto();
    }

    static long readLastAccessedTime(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        return instance != null ? instance.getLastAccessedTime() : 0;
    }

    static void writeLastAccessedTime(int instanceId) {
        long time = TimeUtils.currentTimeMillis();
        assert sData != null;
        putInstance(instanceId, getInstanceFromProto(instanceId).setLastAccessedTime(time));
    }

    static long readClosureTime(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        return instance != null ? instance.getClosureTime() : 0;
    }

    static void writeClosureTime(int instanceId) {
        if (!hasInstance(instanceId)) return;
        long time = TimeUtils.currentTimeMillis();
        assert sData != null;
        putInstance(instanceId, getInstanceFromProto(instanceId).setClosureTime(time));
    }

    static Map<Integer, Integer> readTaskMap() {
        assert sData != null;
        Map<Integer, Integer> taskMap = new HashMap<>();
        for (Map.Entry<Integer, InstanceData> entry : sData.getInstancesMap().entrySet()) {
            if (entry.getValue().hasTaskId()) {
                taskMap.put(entry.getKey(), entry.getValue().getTaskId());
            }
        }
        return taskMap;
    }

    static int readTaskId(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        return (instance != null && instance.hasTaskId())
                ? instance.getTaskId()
                : MultiInstanceManager.INVALID_TASK_ID;
    }

    static void writeTaskId(int instanceId, int taskId) {
        if (!hasInstance(instanceId)) return;
        assert sData != null;
        putInstance(instanceId, getInstanceFromProto(instanceId).setTaskId(taskId));
    }

    static void removeTaskId(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        if (instance != null) {
            putInstance(instanceId, instance.toBuilder().clearTaskId());
        }
    }

    static int readNormalTabCount(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        return instance != null ? instance.getNormalTabCount() : 0;
    }

    static int readIncognitoTabCount(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        return instance != null ? instance.getIncognitoTabCount() : 0;
    }

    static void writeTabCount(int instanceId, int normalTabCount, int incognitoTabCount) {
        if (!hasInstance(instanceId)) return;
        assert sData != null;
        putInstance(
                instanceId,
                getInstanceFromProto(instanceId)
                        .setNormalTabCount(normalTabCount)
                        .setIncognitoTabCount(incognitoTabCount));
    }

    static int readTabCountForRelaunch(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        return instance != null ? instance.getTabCountForRelaunch() : 0;
    }

    static void writeTabCountForRelaunch(int instanceId, int tabCount) {
        if (!hasInstance(instanceId)) return;
        assert sData != null;
        putInstance(instanceId, getInstanceFromProto(instanceId).setTabCountForRelaunch(tabCount));
    }

    static @Nullable String readActiveTabUrl(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        return instance != null ? instance.getActiveTabUrl() : null;
    }

    static void writeActiveTabUrl(int instanceId, String url) {
        if (!hasInstance(instanceId)) return;
        assert sData != null;
        putInstance(instanceId, getInstanceFromProto(instanceId).setActiveTabUrl(url));
    }

    static @Nullable String readActiveTabTitle(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        return instance != null ? instance.getActiveTabTitle() : null;
    }

    static void writeActiveTabTitle(int instanceId, String title) {
        if (!hasInstance(instanceId)) return;
        assert sData != null;
        putInstance(instanceId, getInstanceFromProto(instanceId).setActiveTabTitle(title));
    }

    static @Nullable String readCustomTitle(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        return (instance != null && instance.hasCustomTitle()) ? instance.getCustomTitle() : null;
    }

    static void writeCustomTitle(int instanceId, @Nullable String title) {
        if (!hasInstance(instanceId)) return;
        assert sData != null;
        InstanceData.Builder builder = getInstanceFromProto(instanceId);
        if (title == null) {
            builder.clearCustomTitle();
        } else {
            builder.setCustomTitle(title);
        }
        putInstance(instanceId, builder);
    }

    static @SupportedProfileType int readProfileType(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        return instance != null ? instance.getProfileType() : SupportedProfileType.UNSET;
    }

    static void writeProfileType(int instanceId, @SupportedProfileType int profileType) {
        if (!hasInstance(instanceId)) return;
        // TODO(crbug.com/439670064): Only preserve regular and incognito type until we finalize the
        // upgrade path.
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()
                && (profileType == SupportedProfileType.REGULAR
                        || profileType == SupportedProfileType.OFF_THE_RECORD)) {
            assert sData != null;
            putInstance(instanceId, getInstanceFromProto(instanceId).setProfileType(profileType));
        }
    }

    static boolean containsLatestPersistentStateId(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        return instance != null && instance.hasLatestPersistentStateId();
    }

    static int readLatestPersistentStateId(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        return instance != null ? instance.getLatestPersistentStateId() : INVALID_WINDOW_ID;
    }

    static void writeLatestPersistentStateId(int instanceId, int latestPersistentStateHash) {
        if (!hasInstance(instanceId)) return;
        assert sData != null;
        putInstance(
                instanceId,
                getInstanceFromProto(instanceId)
                        .setLatestPersistentStateId(latestPersistentStateHash));
    }

    static boolean readIncognitoSelected(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        return instance != null ? instance.getIncognitoSelected() : false;
    }

    static void writeIncognitoSelected(int instanceId, boolean incognitoSelected) {
        if (!hasInstance(instanceId)) return;
        assert sData != null;
        putInstance(
                instanceId,
                getInstanceFromProto(instanceId).setIncognitoSelected(incognitoSelected));
    }

    static boolean readMarkedForDeletion(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        return instance != null ? instance.getMarkedForDeletion() : false;
    }

    static void writeMarkedForDeletion(int instanceId, boolean markedForDeletion) {
        if (!hasInstance(instanceId)) return;
        assert sData != null;
        putInstance(
                instanceId,
                getInstanceFromProto(instanceId).setMarkedForDeletion(markedForDeletion));
    }

    static void writeIsVisible(int instanceId, boolean isVisible) {
        if (!hasInstance(instanceId)) return;
        assert sData != null;
        putInstance(instanceId, getInstanceFromProto(instanceId).setIsVisible(isVisible));
    }

    static boolean readIsRecoverable(int instanceId) {
        assert sData != null;
        InstanceData instance = sData.getInstancesMap().get(instanceId);
        return instance != null ? instance.getIsRecoverable() : false;
    }

    static void writeIsRecoverable(int instanceId, boolean isRecoverable) {
        if (MultiWindowUtils.isSessionRestoreAfterCrashEnabled() && hasInstance(instanceId)) {
            assert sData != null;
            putInstance(
                    instanceId, getInstanceFromProto(instanceId).setIsRecoverable(isRecoverable));
        }
    }

    static boolean readIsCrashRecoveryPending() {
        assert sData != null;
        return sData.getIsCrashRecoveryPending();
    }

    static void writeIsCrashRecoveryPending(boolean isCrashRecoveryPending) {
        assert sData != null;
        sData = sData.toBuilder().setIsCrashRecoveryPending(isCrashRecoveryPending).build();
        saveProto();
    }

    static @SessionStartupPolicy int readSessionStartupPolicy() {
        assert sData != null;
        return sData.getSessionStartupPolicy();
    }

    static void writeSessionStartupPolicy(@SessionStartupPolicy int startupPolicy) {
        assert sData != null;
        sData = sData.toBuilder().setSessionStartupPolicy(startupPolicy).build();
        saveProto();
    }

    static void clearSessionStartupPolicy() {
        assert sData != null;
        if (sData.hasSessionStartupPolicy()) {
            sData = sData.toBuilder().clearSessionStartupPolicy().build();
            saveProto();
        }
    }

    static int readRestoreOnStartupPrefValue() {
        assert sData != null;
        return sData.hasRestoreOnStartupPrefValue()
                ? sData.getRestoreOnStartupPrefValue()
                : TabbedStartupWindowPolicyDelegate.PREF_UNSET;
    }

    static void writeRestoreOnStartupPrefValue(int value) {
        assert sData != null;
        sData = sData.toBuilder().setRestoreOnStartupPrefValue(value).build();
        saveProto();
    }

    static List<String> readRestoreOnStartupUrls() {
        assert sData != null;
        return sData.getRestoreOnStartupUrlsList();
    }

    static void writeRestoreOnStartupUrls(List<String> urls) {
        assert sData != null;
        var builder = sData.toBuilder().clearRestoreOnStartupUrls();
        if (!urls.isEmpty()) {
            builder.addAllRestoreOnStartupUrls(urls);
        }
        sData = builder.build();
        saveProto();
    }

    static List<CrashRecoveryWindowInfo> readCrashRecoveryData() {
        assert sData != null;

        List<InstanceDataWithId> crashedInstances = new ArrayList<>();
        for (Map.Entry<Integer, InstanceData> entry : sData.getInstancesMap().entrySet()) {
            int instanceId = entry.getKey();
            InstanceData data = entry.getValue();
            if (data.getIsRecoverable() && !data.getMarkedForDeletion()) {
                crashedInstances.add(new InstanceDataWithId(instanceId, data));
            }
        }

        if (crashedInstances.isEmpty()) return Collections.emptyList();

        // Sort in increasing order of last_accessed_time. This helps restore windows in the
        // required z-order (most recently accessed window is on the top) during post-crash
        // recovery.
        crashedInstances.sort(
                (i1, i2) ->
                        Long.compare(
                                i1.mInstanceData.getLastAccessedTime(),
                                i2.mInstanceData.getLastAccessedTime()));

        List<CrashRecoveryWindowInfo> windows = new ArrayList<>();
        for (InstanceDataWithId item : crashedInstances) {
            windows.add(new CrashRecoveryWindowInfo(item.mId, item.mInstanceData.getIsVisible()));
        }
        return windows;
    }

    private static void putInstance(int instanceId, InstanceData.Builder builder) {
        assert sData != null;
        sData = sData.toBuilder().putInstances(instanceId, builder.build()).build();
        saveProto();
    }

    private static InstanceData.Builder getInstanceFromProto(int instanceId) {
        assumeNonNull(sData);
        return sData
                .getInstancesOrDefault(instanceId, InstanceData.getDefaultInstance())
                .toBuilder();
    }
}
