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
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
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
        MultiInstancePersistentStore.writeActiveTabUrl(instanceId, url);
        MultiInstancePersistentStore.writeLastAccessedTime(instanceId);
        MultiInstancePersistentStore.writeTabCount(
                instanceId, tabCount, /* incognitoTabCount= */ 0);
        MultiInstancePersistentStore.writeTaskId(instanceId, taskId);
    }

    /** Clears instance information. */
    public static void resetInstanceInfo() {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_URL);
        prefs.removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_LAST_ACCESSED_TIME);
        prefs.removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_CLOSURE_TIME);
        prefs.removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT);
        prefs.removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP);
        prefs.removeKey(ChromePreferenceKeys.MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED);
        prefs.removeKey(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_LIMIT);
    }

    /** Enabled multi instance. */
    public static void enableMultiInstance() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
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
                            MultiInstanceManager multiInstanceManager) {
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
