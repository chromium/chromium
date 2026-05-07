// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.INVALID_WINDOW_ID;

import android.graphics.Rect;

import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceDataProto.InstanceData;
import org.chromium.chrome.browser.preferences.MultiInstancePreferenceKeys;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

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
        // We arbitrarily choose to use the lastAccessedTime map to extract persisted instance ids
        // from the SharedPreferences key suffix, from among the SharedPreferences that definitely
        // continue to persist across activity kills / restarts. The taskMap can be cleared when an
        // activity is destroyed and during invalid instance data cleanup which is why we will not
        // use the same to extract ids.
        Set<Integer> ids;
        if (sData != null) {
            ids = sData.getInstancesMap().keySet();
        } else {
            Map<String, Long> lastAccessedTimeMap =
                    getManager()
                            .readLongsWithPrefix(
                                    MultiInstancePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME);
            Pattern pattern = Pattern.compile("(\\d+)$");
            ids = new HashSet<>();
            for (String prefKey : lastAccessedTimeMap.keySet()) {
                Matcher matcher = pattern.matcher(prefKey);
                boolean matchFound = matcher.find();
                assert matchFound : "Key should be suffixed with the instance id.";
                int id = Integer.parseInt(matcher.group(1));
                ids.add(id);
            }
        }

        return ids;
    }

    static boolean hasInstance(int instanceId) {
        return readLastAccessedTime(instanceId) != 0;
    }

    static void deleteInstanceState(int instanceId) {
        if (sData != null) {
            sData = sData.toBuilder().removeInstances(instanceId).build();
            saveProto();
        } else {
            SharedPreferencesManager manager = getManager();
            manager.removeKey(lastAccessedTimeKey(instanceId));
            manager.removeKey(closureTimeKey(instanceId));
            manager.removeKey(taskIdKey(instanceId));
            manager.removeKey(normalTabCountKey(instanceId));
            manager.removeKey(incognitoTabCountKey(instanceId));
            manager.removeKey(tabCountForRelaunchKey(instanceId));
            manager.removeKey(urlKey(instanceId));
            manager.removeKey(activeTabTitleKey(instanceId));
            manager.removeKey(customTitleKey(instanceId));
            manager.removeKey(profileTypeKey(instanceId));
            manager.removeKey(latestPersistentStateIdKey(instanceId));
            manager.removeKey(incognitoSelectedKey(instanceId));
            manager.removeKey(markedForDeletionKey(instanceId));
        }
    }

    static long readLastAccessedTime(int instanceId) {
        if (sData != null) {
            InstanceData instance = sData.getInstancesMap().get(instanceId);
            return instance != null ? instance.getLastAccessedTime() : 0;
        }
        return getManager().readLong(lastAccessedTimeKey(instanceId));
    }

    static void writeLastAccessedTime(int instanceId) {
        long time = TimeUtils.currentTimeMillis();
        if (sData != null) {
            putInstance(instanceId, getInstanceFromProto(instanceId).setLastAccessedTime(time));
        } else {
            getManager().writeLong(lastAccessedTimeKey(instanceId), time);
        }
    }

    static long readClosureTime(int instanceId) {
        if (sData != null) {
            InstanceData instance = sData.getInstancesMap().get(instanceId);
            return instance != null ? instance.getClosureTime() : 0;
        }
        return getManager().readLong(closureTimeKey(instanceId));
    }

    static void writeClosureTime(int instanceId) {
        long time = TimeUtils.currentTimeMillis();
        if (sData != null) {
            putInstance(instanceId, getInstanceFromProto(instanceId).setClosureTime(time));
        } else {
            getManager().writeLong(closureTimeKey(instanceId), time);
        }
    }

    static Map<Integer, Integer> readTaskMap() {
        Map<Integer, Integer> taskMap = new HashMap<>();
        if (sData != null) {
            for (Map.Entry<Integer, InstanceData> entry : sData.getInstancesMap().entrySet()) {
                if (entry.getValue().hasTaskId()) {
                    taskMap.put(entry.getKey(), entry.getValue().getTaskId());
                }
            }
        } else {
            Map<String, Integer> taskIdMap =
                    getManager()
                            .readIntsWithPrefix(
                                    MultiInstancePreferenceKeys.MULTI_INSTANCE_TASK_MAP);
            Pattern pattern = Pattern.compile("(\\d+)$");
            for (Map.Entry<String, Integer> entry : taskIdMap.entrySet()) {
                Matcher matcher = pattern.matcher(entry.getKey());
                if (matcher.find()) {
                    int id = Integer.parseInt(matcher.group(1));
                    taskMap.put(id, entry.getValue());
                }
            }
        }
        return taskMap;
    }

    static int readTaskId(int instanceId) {
        if (sData != null) {
            InstanceData instance = sData.getInstancesMap().get(instanceId);
            return (instance != null && instance.hasTaskId())
                    ? instance.getTaskId()
                    : MultiInstanceManager.INVALID_TASK_ID;
        }
        return getManager().readInt(taskIdKey(instanceId), MultiInstanceManager.INVALID_TASK_ID);
    }

    static void writeTaskId(int instanceId, int taskId) {
        if (sData != null) {
            putInstance(instanceId, getInstanceFromProto(instanceId).setTaskId(taskId));
        } else {
            getManager().writeInt(taskIdKey(instanceId), taskId);
        }
    }

    static void removeTaskId(int instanceId) {
        if (sData != null) {
            InstanceData instance = sData.getInstancesMap().get(instanceId);
            if (instance != null) {
                putInstance(instanceId, instance.toBuilder().clearTaskId());
            }
        } else {
            getManager().removeKey(taskIdKey(instanceId));
        }
    }

    static int readNormalTabCount(int instanceId) {
        if (sData != null) {
            InstanceData instance = sData.getInstancesMap().get(instanceId);
            return instance != null ? instance.getNormalTabCount() : 0;
        }
        return getManager().readInt(normalTabCountKey(instanceId));
    }

    static int readIncognitoTabCount(int instanceId) {
        if (sData != null) {
            InstanceData instance = sData.getInstancesMap().get(instanceId);
            return instance != null ? instance.getIncognitoTabCount() : 0;
        }
        return getManager().readInt(incognitoTabCountKey(instanceId));
    }

    static void writeTabCount(int instanceId, int normalTabCount, int incognitoTabCount) {
        if (sData != null) {
            putInstance(
                    instanceId,
                    getInstanceFromProto(instanceId)
                            .setNormalTabCount(normalTabCount)
                            .setIncognitoTabCount(incognitoTabCount));
        } else {
            getManager().writeInt(normalTabCountKey(instanceId), normalTabCount);
            getManager().writeInt(incognitoTabCountKey(instanceId), incognitoTabCount);
        }
    }

    static int readTabCountForRelaunch(int instanceId) {
        if (sData != null) {
            InstanceData instance = sData.getInstancesMap().get(instanceId);
            return instance != null ? instance.getTabCountForRelaunch() : 0;
        }
        return getManager().readInt(tabCountForRelaunchKey(instanceId));
    }

    static void writeTabCountForRelaunchSync(int instanceId, int tabCount) {
        if (sData != null) {
            putInstance(
                    instanceId, getInstanceFromProto(instanceId).setTabCountForRelaunch(tabCount));
        } else {
            getManager().writeIntSync(tabCountForRelaunchKey(instanceId), tabCount);
        }
    }

    static @Nullable String readActiveTabUrl(int instanceId) {
        if (sData != null) {
            InstanceData instance = sData.getInstancesMap().get(instanceId);
            return instance != null ? instance.getActiveTabUrl() : null;
        }
        return getManager().readString(urlKey(instanceId), null);
    }

    static void writeActiveTabUrl(int instanceId, String url) {
        if (sData != null) {
            putInstance(instanceId, getInstanceFromProto(instanceId).setActiveTabUrl(url));
        } else {
            getManager().writeString(urlKey(instanceId), url);
        }
    }

    static @Nullable String readActiveTabTitle(int instanceId) {
        if (sData != null) {
            InstanceData instance = sData.getInstancesMap().get(instanceId);
            return instance != null ? instance.getActiveTabTitle() : null;
        }
        return getManager().readString(activeTabTitleKey(instanceId), null);
    }

    static void writeActiveTabTitle(int instanceId, String title) {
        if (sData != null) {
            putInstance(instanceId, getInstanceFromProto(instanceId).setActiveTabTitle(title));
        } else {
            getManager().writeString(activeTabTitleKey(instanceId), title);
        }
    }

    static @Nullable String readCustomTitle(int instanceId) {
        if (sData != null) {
            InstanceData instance = sData.getInstancesMap().get(instanceId);
            return (instance != null && instance.hasCustomTitle())
                    ? instance.getCustomTitle()
                    : null;
        }
        return getManager().readString(customTitleKey(instanceId), null);
    }

    static void writeCustomTitle(int instanceId, @Nullable String title) {
        if (sData != null) {
            InstanceData.Builder builder = getInstanceFromProto(instanceId);
            if (title == null) {
                builder.clearCustomTitle();
            } else {
                builder.setCustomTitle(title);
            }
            putInstance(instanceId, builder);
        } else {
            getManager().writeString(customTitleKey(instanceId), title);
        }
    }

    static @SupportedProfileType int readProfileType(int instanceId) {
        if (sData != null) {
            InstanceData instance = sData.getInstancesMap().get(instanceId);
            return instance != null ? instance.getProfileType() : SupportedProfileType.UNSET;
        }
        return getManager().readInt(profileTypeKey(instanceId), SupportedProfileType.UNSET);
    }

    static void writeProfileType(int instanceId, @SupportedProfileType int profileType) {
        // TODO(crbug.com/439670064): Only preserve regular and incognito type until we finalize the
        // upgrade path.
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()
                && (profileType == SupportedProfileType.REGULAR
                        || profileType == SupportedProfileType.OFF_THE_RECORD)) {
            if (sData != null) {
                putInstance(
                        instanceId, getInstanceFromProto(instanceId).setProfileType(profileType));
            } else {
                getManager().writeInt(profileTypeKey(instanceId), profileType);
            }
        }
    }

    static boolean containsLatestPersistentStateId(int instanceId) {
        if (sData != null) {
            InstanceData instance = sData.getInstancesMap().get(instanceId);
            return instance != null && instance.hasLatestPersistentStateId();
        }
        return getManager().contains(latestPersistentStateIdKey(instanceId));
    }

    static int readLatestPersistentStateId(int instanceId) {
        if (sData != null) {
            InstanceData instance = sData.getInstancesMap().get(instanceId);
            return instance != null ? instance.getLatestPersistentStateId() : INVALID_WINDOW_ID;
        }
        return getManager().readInt(latestPersistentStateIdKey(instanceId));
    }

    static void writeLatestPersistentStateId(int instanceId, int latestPersistentStateHash) {
        if (sData != null) {
            putInstance(
                    instanceId,
                    getInstanceFromProto(instanceId)
                            .setLatestPersistentStateId(latestPersistentStateHash));
        } else {
            getManager()
                    .writeInt(latestPersistentStateIdKey(instanceId), latestPersistentStateHash);
        }
    }

    static boolean readIncognitoSelected(int instanceId) {
        if (sData != null) {
            InstanceData instance = sData.getInstancesMap().get(instanceId);
            return instance != null ? instance.getIncognitoSelected() : false;
        }
        return getManager().readBoolean(incognitoSelectedKey(instanceId), false);
    }

    static void writeIncognitoSelected(int instanceId, boolean incognitoSelected) {
        if (sData != null) {
            putInstance(
                    instanceId,
                    getInstanceFromProto(instanceId).setIncognitoSelected(incognitoSelected));
        } else {
            getManager().writeBoolean(incognitoSelectedKey(instanceId), incognitoSelected);
        }
    }

    static boolean readMarkedForDeletion(int instanceId) {
        if (sData != null) {
            InstanceData instance = sData.getInstancesMap().get(instanceId);
            return instance != null ? instance.getMarkedForDeletion() : false;
        }
        return getManager().readBoolean(markedForDeletionKey(instanceId), false);
    }

    static void writeMarkedForDeletion(int instanceId, boolean markedForDeletion) {
        if (sData != null) {
            putInstance(
                    instanceId,
                    getInstanceFromProto(instanceId).setMarkedForDeletion(markedForDeletion));
        } else {
            getManager().writeBoolean(markedForDeletionKey(instanceId), markedForDeletion);
        }
    }

    static void writeIsVisible(int instanceId, boolean isVisible) {
        if (sData != null) {
            putInstance(instanceId, getInstanceFromProto(instanceId).setIsVisible(isVisible));
        }
    }

    static void writeBounds(int instanceId, Rect bounds) {
        if (sData != null) {
            InstanceData.Rect protoRect =
                    InstanceData.Rect.newBuilder()
                            .setLeft(bounds.left)
                            .setTop(bounds.top)
                            .setRight(bounds.right)
                            .setBottom(bounds.bottom)
                            .build();
            putInstance(instanceId, getInstanceFromProto(instanceId).setBounds(protoRect));
        }
    }

    static void writeIsRecoverable(int instanceId, boolean isRecoverable) {
        if (sData != null) {
            putInstance(
                    instanceId, getInstanceFromProto(instanceId).setIsRecoverable(isRecoverable));
        }
    }

    static boolean readIsCrashRecoveryPending() {
        return sData != null && sData.getIsCrashRecoveryPending();
    }

    static void writeIsCrashRecoveryPending(boolean isCrashRecoveryPending) {
        if (sData != null) {
            sData = sData.toBuilder().setIsCrashRecoveryPending(isCrashRecoveryPending).build();
            saveProto();
        }
    }

    static List<CrashRecoveryWindowInfo> readCrashRecoveryData() {
        if (sData == null) return Collections.emptyList();

        List<InstanceDataWithId> crashedInstances = new ArrayList<>();
        for (Map.Entry<Integer, InstanceData> entry : sData.getInstancesMap().entrySet()) {
            InstanceData data = entry.getValue();
            if (data.getIsRecoverable()) {
                crashedInstances.add(new InstanceDataWithId(entry.getKey(), data));
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
            Rect bounds = null;
            if (item.mInstanceData.hasBounds()) {
                InstanceData.Rect protoBounds = item.mInstanceData.getBounds();
                bounds =
                        new Rect(
                                protoBounds.getLeft(),
                                protoBounds.getTop(),
                                protoBounds.getRight(),
                                protoBounds.getBottom());
            }
            windows.add(
                    new CrashRecoveryWindowInfo(
                            item.mId, bounds, item.mInstanceData.getIsVisible()));
        }
        return windows;
    }

    private static String lastAccessedTimeKey(int instanceId) {
        return MultiInstancePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME.createKey(
                String.valueOf(instanceId));
    }

    private static String closureTimeKey(int instanceId) {
        return MultiInstancePreferenceKeys.MULTI_INSTANCE_CLOSURE_TIME.createKey(
                String.valueOf(instanceId));
    }

    private static String taskIdKey(int instanceId) {
        return MultiInstancePreferenceKeys.MULTI_INSTANCE_TASK_MAP.createKey(
                String.valueOf(instanceId));
    }

    private static String normalTabCountKey(int instanceId) {
        return MultiInstancePreferenceKeys.MULTI_INSTANCE_TAB_COUNT.createKey(
                String.valueOf(instanceId));
    }

    private static String incognitoTabCountKey(int instanceId) {
        return MultiInstancePreferenceKeys.MULTI_INSTANCE_INCOGNITO_TAB_COUNT.createKey(
                String.valueOf(instanceId));
    }

    private static String tabCountForRelaunchKey(int instanceId) {
        return MultiInstancePreferenceKeys.MULTI_INSTANCE_TAB_COUNT_FOR_RELAUNCH.createKey(
                String.valueOf(instanceId));
    }

    private static String urlKey(int instanceId) {
        return MultiInstancePreferenceKeys.MULTI_INSTANCE_URL.createKey(String.valueOf(instanceId));
    }

    private static String activeTabTitleKey(int instanceId) {
        return MultiInstancePreferenceKeys.MULTI_INSTANCE_TITLE.createKey(
                String.valueOf(instanceId));
    }

    private static String customTitleKey(int instanceId) {
        return MultiInstancePreferenceKeys.MULTI_INSTANCE_CUSTOM_TITLE.createKey(
                String.valueOf(instanceId));
    }

    private static String profileTypeKey(int instanceId) {
        return MultiInstancePreferenceKeys.MULTI_INSTANCE_PROFILE_TYPE.createKey(
                String.valueOf(instanceId));
    }

    private static String latestPersistentStateIdKey(int instanceId) {
        return MultiInstancePreferenceKeys.MULTI_INSTANCE_LATEST_PERSISTENT_STATE_ID.createKey(
                String.valueOf(instanceId));
    }

    private static String incognitoSelectedKey(int instanceId) {
        return MultiInstancePreferenceKeys.MULTI_INSTANCE_IS_INCOGNITO_SELECTED.createKey(
                String.valueOf(instanceId));
    }

    private static String markedForDeletionKey(int instanceId) {
        return MultiInstancePreferenceKeys.MULTI_INSTANCE_MARKED_FOR_DELETION.createKey(
                String.valueOf(instanceId));
    }

    private static void putInstance(int instanceId, InstanceData.Builder builder) {
        if (sData == null) return;
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
