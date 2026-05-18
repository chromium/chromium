// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.content.Context;
import android.util.Pair;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.preferences.MultiInstancePreferenceKeys;
import org.chromium.chrome.browser.preferences.MultiInstanceSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabModelSelectorFactory;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Test util methods for multi-window/instance support */
public class MultiWindowTestUtils {
    /**
     * Create a new instance information.
     *
     * @param instanceId Instance (aka window) ID.
     * @param url URL for the active tab.
     * @param tabCount The number of tabs in the instance.
     * @param taskId ID of the task the activity instance runs in.
     */
    public static void createInstance(int instanceId, String url, int tabCount, int taskId) {
        ChromeMultiInstancePersistentStore.writeActiveTabUrl(instanceId, url);
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(instanceId);
        ChromeMultiInstancePersistentStore.writeTabCount(
                instanceId, tabCount, /* incognitoTabCount= */ 0);
        ChromeMultiInstancePersistentStore.writeTaskId(instanceId, taskId);
        if (taskId != -1) MultiWindowUtils.addAppTaskIdForTesting(taskId);
    }

    /**
     * Create test persisted instance state.
     *
     * @param numActive The number of active instances to create.
     * @param numInactive The number of inactive instances to create.
     * @param profileType The {@link SupportedProfileType} of the instances.
     * @param startId The first instance id to use for the set of instances. Active Instances will
     *     be created starting with this id followed by inactive instances with values incremented
     *     by 1 for each persisted instance.
     */
    public static void createInstances(
            int numActive, int numInactive, @SupportedProfileType int profileType, int startId) {
        int start = startId;
        int end = start + numActive;
        for (int i = start; i < end; i++) {
            createInstance(
                    /* instanceId= */ i,
                    /* url= */ "www.example.com",
                    /* tabCount= */ 2,
                    /* taskId= */ i);
            ChromeMultiInstancePersistentStore.writeProfileType(i, profileType);
        }

        start = startId + numActive;
        end = start + numInactive;
        for (int i = start; i < end; i++) {
            createInstance(
                    /* instanceId= */ i,
                    /* url= */ "www.example.com",
                    /* tabCount= */ 2,
                    /* taskId= */ -1);
            ChromeMultiInstancePersistentStore.writeProfileType(i, profileType);
        }
    }

    /** Clears instance information. */
    public static void resetInstanceInfo() {
        MultiInstancePersistentStore.resetForTesting();
        SharedPreferencesManager prefs = MultiInstanceSharedPreferences.getInstance();
        prefs.removeKeysWithPrefix(MultiInstancePreferenceKeys.MULTI_INSTANCE_URL);
        prefs.removeKeysWithPrefix(MultiInstancePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME);
        prefs.removeKeysWithPrefix(MultiInstancePreferenceKeys.MULTI_INSTANCE_CLOSURE_TIME);
        prefs.removeKeysWithPrefix(MultiInstancePreferenceKeys.MULTI_INSTANCE_TAB_COUNT);
        prefs.removeKeysWithPrefix(MultiInstancePreferenceKeys.MULTI_INSTANCE_TASK_MAP);
        prefs.removeKey(
                MultiInstancePreferenceKeys.MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED);
        prefs.removeKey(MultiInstancePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_LIMIT);
    }

    /** Enabled multi instance. */
    public static void enableMultiInstance() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        MultiInstancePersistentStore.ensureInitialized();
    }

    /* package */ static void setupTabModelSelectorFactory(
            Profile regularProfile, Profile incognitoProfile) {
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
        TabWindowManagerSingleton.setTabModelSelectorFactoryForTesting(
                new TabModelSelectorFactory() {
                    @Override
                    public TabModelSelector buildTabbedSelector(
                            Context context,
                            ModalDialogManager modalDialogManager,
                            OneshotSupplier<ProfileProvider> profileProviderSupplier,
                            TabCreatorManager tabCreatorManager,
                            NextTabPolicySupplier nextTabPolicySupplier,
                            @SupportedProfileType int supportedProfileType) {
                        return new MockTabModelSelector(
                                regularProfile, incognitoProfile, 0, 0, null);
                    }

                    @Override
                    public Pair<TabModelSelector, Destroyable> buildHeadlessSelector(
                            @WindowId int windowId, Profile profile) {
                        return Pair.create(
                                new MockTabModelSelector(
                                        regularProfile,
                                        incognitoProfile,
                                        /* tabCount= */ 0,
                                        /* incognitoTabCount= */ 0,
                                        /* delegate= */ null),
                                () -> {});
                    }
                });
    }
}
