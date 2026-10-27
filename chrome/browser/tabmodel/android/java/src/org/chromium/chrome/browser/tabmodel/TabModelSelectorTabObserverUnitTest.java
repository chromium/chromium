// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;

import java.util.HashSet;
import java.util.Set;

/** Tests for the TabModelSelectorTabObserver. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabModelSelectorTabObserverUnitTest {
    private static final long FAKE_NATIVE_ADDRESS = 123L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelJniBridge.Natives mTabModelJniBridge;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;

    private TabModelSelector mTabModelSelector;

    @Before
    public void setUp() {
        TabModelJniBridgeJni.setInstanceForTesting(mTabModelJniBridge);
        when(mTabModelJniBridge.init(
                        any(TabModelJniBridge.class),
                        any(Profile.class),
                        anyInt(),
                        any(),
                        anyInt()))
                .thenReturn(FAKE_NATIVE_ADDRESS);

        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        TestTabModelSelector selector = new TestTabModelSelector(mTabCreatorManager);
        MockTabModel normalTabModel = new MockTabModel(mProfile, null);
        normalTabModel.setTabRemoverForTesting(createTabRemover(normalTabModel));
        MockTabModel incognitoTabModel = new MockTabModel(mIncognitoProfile, null);
        incognitoTabModel.setTabRemoverForTesting(createTabRemover(incognitoTabModel));
        selector.initialize(normalTabModel, incognitoTabModel);
        mTabModelSelector = selector;
    }

    private static TabRemover createTabRemover(MockTabModel tabModel) {
        return new TabRemover() {
            @Override
            public void closeTabs(
                    TabClosureParams tabClosureParams,
                    boolean allowDialog,
                    @Nullable TabModelActionListener listener) {
                forceCloseTabs(tabClosureParams);
            }

            @Override
            public void prepareCloseTabs(
                    TabClosureParams tabClosureParams,
                    boolean allowDialog,
                    @Nullable TabModelActionListener listener,
                    Callback<TabClosureParams> onPreparedCallback) {
                fail("Not reached");
            }

            @Override
            public void forceCloseTabs(TabClosureParams tabClosureParams) {
                for (Tab tab : tabClosureParams.tabs) {
                    tabModel.removeTab(tab);
                }
            }

            @Override
            public void removeTab(
                    Tab tab, boolean allowDialog, @Nullable TabModelActionListener listener) {
                tabModel.removeTab(tab);
            }
        };
    }

    /**
     * Destroying the observer before its posted initial-registration task runs must not deliver
     * {@code onTabUnregistered} for tabs that never received {@code onTabRegistered}, nor deliver
     * {@code onTabRegistered} when the posted task later executes.
     */
    @Test
    public void testDestroyBeforeDeferredRegistrationRuns() {
        Tab preExistingTab = MockTab.createAndInitialize(1, mProfile);
        addTabs(preExistingTab);

        RecordingObserver observer = new RecordingObserver(mTabModelSelector);
        assertTrue(tabHasObserver(preExistingTab, observer));
        assertTrue(observer.mRegisteredTabs.isEmpty());

        // Destroy in the same UI turn before the postAtFrontOfQueue task executes.
        observer.destroy();

        assertTrue(observer.mUnregisteredTabs.isEmpty());
        assertFalse(tabHasObserver(preExistingTab, observer));

        // Running the posted task after destroy() must not deliver late registrations.
        ShadowLooper.runUiThreadTasks();
        assertTrue(observer.mRegisteredTabs.isEmpty());
        assertTrue(observer.mUnregisteredTabs.isEmpty());
    }

    /**
     * Removing a pre-existing tab before the posted initial-registration task runs skips both
     * {@code onTabUnregistered} and the deferred {@code onTabRegistered} for that tab, while still
     * registering remaining pre-existing tabs.
     */
    @Test
    public void testRemovePreExistingTabBeforeDeferredRegistrationRuns() {
        Tab removedTab = MockTab.createAndInitialize(1, mProfile);
        Tab keptTab = MockTab.createAndInitialize(2, mProfile);
        addTabs(removedTab, keptTab);

        RecordingObserver observer = new RecordingObserver(mTabModelSelector);
        mTabModelSelector
                .getModel(false)
                .getTabRemover()
                .removeTab(removedTab, /* allowDialog= */ false);

        assertTrue(observer.mUnregisteredTabs.isEmpty());
        assertFalse(tabHasObserver(removedTab, observer));

        ShadowLooper.runUiThreadTasks();
        assertTrue(observer.isDeferredInitializationFinishedForTesting());
        assertFalse(observer.mRegisteredTabs.contains(removedTab));
        assertTrue(observer.mRegisteredTabs.contains(keptTab));

        observer.destroy();
        assertFalse(observer.mUnregisteredTabs.contains(removedTab));
        assertTrue(observer.mUnregisteredTabs.contains(keptTab));
    }

    /**
     * A pre-existing tab destroyed while still in the model, before the posted initial-registration
     * task runs, gets neither {@code onTabRegistered} nor a later {@code onTabUnregistered}.
     */
    @Test
    public void testDestroyPreExistingTabBeforeDeferredRegistrationRuns() {
        Tab destroyedTab = MockTab.createAndInitialize(1, mProfile);
        Tab keptTab = MockTab.createAndInitialize(2, mProfile);
        addTabs(destroyedTab, keptTab);

        RecordingObserver observer = new RecordingObserver(mTabModelSelector);
        destroyedTab.destroy();

        ShadowLooper.runUiThreadTasks();
        assertTrue(observer.isDeferredInitializationFinishedForTesting());
        assertFalse(observer.mRegisteredTabs.contains(destroyedTab));
        assertTrue(observer.mRegisteredTabs.contains(keptTab));

        // destroyedTab is still in the model, so destroy() unregisters it and must skip
        // onTabUnregistered.
        observer.destroy();
        assertFalse(observer.mUnregisteredTabs.contains(destroyedTab));
        assertTrue(observer.mUnregisteredTabs.contains(keptTab));
    }

    /**
     * Removing a still-deferred tab from {@code onTabRegistered} skips both callbacks for that tab
     * and still registers the remaining deferred tabs.
     */
    @Test
    public void testRemoveDeferredTabFromOnTabRegistered() {
        Tab firstTab = MockTab.createAndInitialize(1, mProfile);
        Tab removedTab = MockTab.createAndInitialize(2, mProfile);
        Tab lastTab = MockTab.createAndInitialize(3, mProfile);
        addTabs(firstTab, removedTab, lastTab);

        TabRemover tabRemover = mTabModelSelector.getModel(false).getTabRemover();
        RecordingObserver observer =
                new RecordingObserver(mTabModelSelector) {
                    @Override
                    protected void onTabRegistered(Tab tab) {
                        super.onTabRegistered(tab);
                        if (tab == firstTab) {
                            tabRemover.removeTab(removedTab, /* allowDialog= */ false);
                        }
                    }
                };

        ShadowLooper.runUiThreadTasks();
        assertTrue(observer.mRegisteredTabs.contains(firstTab));
        assertFalse(observer.mRegisteredTabs.contains(removedTab));
        assertTrue(observer.mRegisteredTabs.contains(lastTab));
        assertTrue(observer.mUnregisteredTabs.isEmpty());
    }

    private void addTabs(Tab... tabs) {
        TabModel model = mTabModelSelector.getModel(false);
        for (Tab tab : tabs) {
            model.addTab(
                    tab,
                    model.getCount(),
                    TabLaunchType.FROM_LINK,
                    TabCreationState.LIVE_IN_FOREGROUND);
        }
    }

    private static boolean tabHasObserver(Tab tab, TabObserver target) {
        for (TabObserver observer : TabTestUtils.getTabObservers(tab)) {
            if (observer == target) return true;
        }
        return false;
    }

    private static class RecordingObserver extends TabModelSelectorTabObserver {
        final Set<Tab> mRegisteredTabs = new HashSet<>();
        final Set<Tab> mUnregisteredTabs = new HashSet<>();

        RecordingObserver(TabModelSelector selector) {
            super(selector);
        }

        @Override
        protected void onTabRegistered(Tab tab) {
            assertTrue(mRegisteredTabs.add(tab));
        }

        @Override
        protected void onTabUnregistered(Tab tab) {
            assertTrue(mRegisteredTabs.contains(tab));
            assertTrue(mUnregisteredTabs.add(tab));
        }
    }

    private static class TestTabModelSelector extends TabModelSelectorBase {
        TestTabModelSelector(TabCreatorManager tabCreatorManager) {
            super(tabCreatorManager, false);
        }

        @Override
        public void requestToShowTab(Tab tab, int type) {}

        @Override
        public @Nullable Profile getProfile(boolean offTheRecord) {
            return null;
        }
    }
}
