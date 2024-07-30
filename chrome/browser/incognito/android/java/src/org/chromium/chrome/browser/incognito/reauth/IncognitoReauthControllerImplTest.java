// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_ui.TabSwitcherCustomViewManager;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/** Unit tests for {@link IncognitoReauthControllerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class IncognitoReauthControllerImplTest {
    public static final int TASK_ID = 123;

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcherMock;
    @Mock private LayoutStateProvider mLayoutStateProviderMock;
    @Mock private TabModelSelector mTabModelSelectorMock;
    @Mock private TabModel mIncognitoTabModelMock;
    @Mock private TabModel mRegularTabModelMock;
    @Mock private Profile mProfileMock;
    @Mock private IncognitoReauthCoordinatorFactory mIncognitoReauthCoordinatorFactoryMock;
    @Mock private IncognitoReauthCoordinator mIncognitoReauthCoordinatorMock;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private Runnable mBackPressInReauthFullScreenRunnableMock;
    @Mock private IncognitoReauthManager.IncognitoReauthCallback mIncognitoReauthCallbackMock;
    @Mock private TabSwitcherCustomViewManager mTabSwitcherCustomViewManager;

    @Captor ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Captor ArgumentCaptor<IncognitoTabModelObserver> mIncognitoTabModelObserverCaptor;

    @Captor
    ArgumentCaptor<LayoutStateProvider.LayoutStateObserver> mLayoutStateObserverArgumentCaptor;

    private IncognitoReauthControllerImpl mIncognitoReauthController;
    private OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderOneshotSupplier;
    private OneshotSupplierImpl<TabSwitcherCustomViewManager>
            mTabSwitcherCustomViewManagerOneshotSupplier;
    private boolean mCustomViewManagerHasValue;
    private ObservableSupplierImpl<Profile> mProfileObservableSupplier;

    private boolean mIsIncognitoReauthPendingOnRestore;

    private void switchToIncognitoTabModel() {
        doReturn(true).when(mTabModelSelectorMock).isIncognitoBrandedModelSelected();
        mIncognitoReauthController.onBeforeIncognitoTabModelSelected();
    }

    private void switchToRegularTabModel() {
        doReturn(false).when(mTabModelSelectorMock).isIncognitoBrandedModelSelected();
        mIncognitoReauthController.onAfterRegularTabModelChanged();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfileMock)).thenReturn(mPrefServiceMock);
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(true);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(true);

        doReturn(false).when(mTabModelSelectorMock).isTabStateInitialized();
        doReturn(false).when(mTabModelSelectorMock).isIncognitoBrandedModelSelected();

        doNothing()
                .when(mTabModelSelectorMock)
                .addObserver(mTabModelSelectorObserverCaptor.capture());
        doNothing()
                .when(mTabModelSelectorMock)
                .addIncognitoTabModelObserver(mIncognitoTabModelObserverCaptor.capture());
        doNothing().when(mTabModelSelectorMock).setIncognitoReauthDialogDelegate(any());
        doNothing().when(mActivityLifecycleDispatcherMock).register(any());

        doReturn(mIncognitoTabModelMock)
                .when(mTabModelSelectorMock)
                .getModel(/* incognito= */ true);
        doReturn(0).when(mIncognitoTabModelMock).getCount();
        doReturn(true).when(mIncognitoTabModelMock).isIncognito();
        doReturn(false).when(mRegularTabModelMock).isIncognito();
        doReturn(false).when(mLayoutStateProviderMock).isLayoutVisible(LayoutType.TAB_SWITCHER);
        doReturn(mIncognitoReauthCoordinatorMock)
                .when(mIncognitoReauthCoordinatorFactoryMock)
                .createIncognitoReauthCoordinator(any(), /* showFullScreen= */ anyBoolean(), any());
        doReturn(true).when(mIncognitoReauthCoordinatorFactoryMock).getIsTabbedActivity();
        doNothing().when(mIncognitoReauthCoordinatorMock).show();

        mTabSwitcherCustomViewManagerOneshotSupplier = new OneshotSupplierImpl<>();
        doReturn(mTabSwitcherCustomViewManagerOneshotSupplier)
                .when(mIncognitoReauthCoordinatorFactoryMock)
                .getTabSwitcherCustomViewManagerSupplier();
        doReturn(true)
                .when(mIncognitoReauthCoordinatorFactoryMock)
                .areDependenciesReadyFor(anyBoolean());

        mLayoutStateProviderOneshotSupplier = new OneshotSupplierImpl<>();
        mLayoutStateProviderOneshotSupplier.set(mLayoutStateProviderMock);

        mProfileObservableSupplier = new ObservableSupplierImpl<>();

        mIncognitoReauthController =
                new IncognitoReauthControllerImpl(
                        mTabModelSelectorMock,
                        mActivityLifecycleDispatcherMock,
                        mLayoutStateProviderOneshotSupplier,
                        mProfileObservableSupplier,
                        mIncognitoReauthCoordinatorFactoryMock,
                        () -> mIsIncognitoReauthPendingOnRestore,
                        TASK_ID);
        mProfileObservableSupplier.set(mProfileMock);

        verify(mLayoutStateProviderMock, times(1))
                .addObserver(mLayoutStateObserverArgumentCaptor.capture());
    }

    @After
    public void tearDown() {
        doNothing().when(mActivityLifecycleDispatcherMock).unregister(any());
        doNothing()
                .when(mTabModelSelectorMock)
                .removeObserver(mTabModelSelectorObserverCaptor.capture());
        doNothing()
                .when(mTabModelSelectorMock)
                .removeIncognitoTabModelObserver(mIncognitoTabModelObserverCaptor.capture());
        mIncognitoReauthController.destroy();

        verify(mActivityLifecycleDispatcherMock, times(1)).unregister(any());
        verify(mTabModelSelectorMock, times(1))
                .removeIncognitoTabModelObserver(mIncognitoTabModelObserverCaptor.capture());
    }

    /**
     * This tests that we don't show a re-auth for freshly created Incognito tabs where Chrome has
     * not been backgrounded yet.
     */
    @Test
    @MediumTest
    public void testIncognitoTabsCreated_BeforeBackground_DoesNotShowReauth() {
        // Pretend there's one incognito tab.
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();

        assertFalse(
                "IncognitoReauthCoordinator should not be created for fresh Incognito"
                        + " session when Chrome has not been backgrounded yet.",
                mIncognitoReauthController.isReauthPageShowing());
    }

    /**
     * This tests that we do show a re-auth when Incognito tabs already exists after Chrome comes to
     * foreground.
     */
    @Test
    @MediumTest
    public void testIncognitoTabsCreated_BeforeForeground_ShowsReauth() {
        // Pretend there's one incognito tab.
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();

        mIncognitoReauthController.onTaskVisibilityChanged(TASK_ID, false);
        mIncognitoReauthController.onStartWithNative();

        assertTrue(
                "IncognitoReauthCoordinator should be created when Incognito tabs"
                        + " exists already after coming to foreground.",
                mIncognitoReauthController.isReauthPageShowing());
        verify(mIncognitoReauthCoordinatorMock).show();
    }

    @Test
    @MediumTest
    public void testRegularTabModel_DoesNotShowReauth() {
        switchToRegularTabModel();
        mIncognitoReauthController.onTaskVisibilityChanged(TASK_ID, false);
        mIncognitoReauthController.onStartWithNative();

        assertFalse(
                "IncognitoReauthCoordinator should not be created on regular" + " TabModel.",
                mIncognitoReauthController.isReauthPageShowing());
    }

    @Test
    @MediumTest
    public void testIncognitoTabsExisting_AndChromeForegroundedWithRegularTabs_DoesNotShowReauth() {
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        doReturn(false).when(mTabModelSelectorMock).isIncognitoBrandedModelSelected();
        mIncognitoReauthController.onTaskVisibilityChanged(TASK_ID, false);
        mIncognitoReauthController.onStartWithNative();

        assertFalse(
                "IncognitoReauthCoordinator should not be created on regular" + " TabModel.",
                mIncognitoReauthController.isReauthPageShowing());
    }

    @Test
    @MediumTest
    public void testWhenTabModelChangesToRegularFromIncognito_HidesReauth() {
        // Pretend there's one incognito tab.
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();
        assertFalse(
                "IncognitoReauthCoordinator should not be created if Chrome has not"
                        + " been to background.",
                mIncognitoReauthController.isReauthPageShowing());

        // Chrome went to background.
        mIncognitoReauthController.onTaskVisibilityChanged(TASK_ID, false);
        // Chrome coming to foregrounded. Re-auth would now be required since there are existing
        // Incognito tabs.
        mIncognitoReauthController.onStartWithNative();
        assertTrue(
                "IncognitoReauthCoordinator should have been created.",
                mIncognitoReauthController.isReauthPageShowing());
        verify(mIncognitoReauthCoordinatorMock).show();

        switchToRegularTabModel();
        assertFalse(
                "IncognitoReauthCoordinator should have been destroyed"
                        + "when a user switches to regular TabModel.",
                mIncognitoReauthController.isReauthPageShowing());
    }

    @Test
    @MediumTest
    public void testIncognitoTabsRestore_ShowsReauth() {
        // Pretend there's one incognito tab.
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();
        mIsIncognitoReauthPendingOnRestore = true;

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        assertTrue(
                "IncognitoReauthCoordinator should be created for restored Incognito tabs.",
                mIncognitoReauthController.isReauthPageShowing());
        verify(mIncognitoReauthCoordinatorMock).show();
    }

    @Test
    @MediumTest
    public void testTabSwitcherCustomViewManagerSupplied_ShowsReauth() {
        doReturn(true).when(mLayoutStateProviderMock).isLayoutVisible(LayoutType.TAB_SWITCHER);
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();
        mIncognitoReauthController.onTaskVisibilityChanged(TASK_ID, false);

        mTabSwitcherCustomViewManagerOneshotSupplier.set(mTabSwitcherCustomViewManager);
        assertTrue(
                "IncognitoReauthCoordinator should be created for tab switcher custom view manager",
                mIncognitoReauthController.isReauthPageShowing());
        verify(mIncognitoReauthCoordinatorFactoryMock).areDependenciesReadyFor(false);
        verify(mIncognitoReauthCoordinatorMock).show();
    }

    @Test
    @MediumTest
    public void testTabSwitcherCustomViewManagerMissing_DoesNotShowsReauth() {
        doReturn(false)
                .when(mIncognitoReauthCoordinatorFactoryMock)
                .areDependenciesReadyFor(anyBoolean());

        doReturn(true).when(mLayoutStateProviderMock).isLayoutVisible(LayoutType.TAB_SWITCHER);
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();
        mIncognitoReauthController.onTaskVisibilityChanged(TASK_ID, false);

        mIncognitoReauthController.onStartWithNative();
        assertFalse(
                "IncognitoReauthCoordinator should not be created if custom view manager missing.",
                mIncognitoReauthController.isReauthPageShowing());
        verify(mIncognitoReauthCoordinatorFactoryMock).areDependenciesReadyFor(false);
    }

    @Test
    @MediumTest
    public void testIncognitoTabsFromLauncherShortcut_DoesNotShowReauth() {
        // Pretend there's one incognito tab.
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();
        mIsIncognitoReauthPendingOnRestore = false;

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        assertFalse(
                "IncognitoReauthCoordinator should not be created for Incognito tabs"
                        + " opened from launcher.",
                mIncognitoReauthController.isReauthPageShowing());
    }

    @Test
    @MediumTest
    public void testNewIncognitoSession_AfterClosingIncognitoTabs_DoesNotShowReauth() {
        // Pretend there's one incognito tab.
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();
        assertFalse(
                "IncognitoReauthCoordinator should not be created if Chrome has not"
                        + " been to background.",
                mIncognitoReauthController.isReauthPageShowing());

        // Chrome went to background.
        mIncognitoReauthController.onTaskVisibilityChanged(TASK_ID, false);
        // Chrome coming to foregrounded. Re-auth would now be required since there are existing
        // Incognito tabs.
        doReturn(true).when(mTabModelSelectorMock).isIncognitoBrandedModelSelected();
        mIncognitoReauthController.onStartWithNative();
        switchToIncognitoTabModel();
        assertTrue(
                "IncognitoReauthCoordinator should be created when all conditions are" + " met.",
                mIncognitoReauthController.isReauthPageShowing());
        verify(mIncognitoReauthCoordinatorMock).show();

        // Move to regular mode.
        switchToRegularTabModel();
        // close all Incognito tabs.
        mIncognitoTabModelObserverCaptor.getValue().didBecomeEmpty();
        // Open an Incognito tab.
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();
        assertFalse(
                "IncognitoReauthCoordinator should not be created when starting a"
                        + " fresh Incognito session.",
                mIncognitoReauthController.isReauthPageShowing());
    }

    @Test
    @SmallTest
    public void testAddIncognitoReauthCallback_IsHookedWithMainCallback() {
        doNothing().when(mIncognitoReauthCallbackMock).onIncognitoReauthSuccess();
        mIncognitoReauthController.addIncognitoReauthCallback(mIncognitoReauthCallbackMock);
        mIncognitoReauthController
                .getIncognitoReauthCallbackForTesting()
                .onIncognitoReauthSuccess();
        verify(mIncognitoReauthCallbackMock, times(1)).onIncognitoReauthSuccess();
    }

    @Test
    @SmallTest
    public void testRemoveIncognitoReauthCallback_IsUnHookedWithMainCallback() {
        doNothing().when(mIncognitoReauthCallbackMock).onIncognitoReauthSuccess();
        mIncognitoReauthController.addIncognitoReauthCallback(mIncognitoReauthCallbackMock);
        mIncognitoReauthController
                .getIncognitoReauthCallbackForTesting()
                .onIncognitoReauthSuccess();
        verify(mIncognitoReauthCallbackMock, times(1)).onIncognitoReauthSuccess();

        mIncognitoReauthController.removeIncognitoReauthCallback(mIncognitoReauthCallbackMock);
        mIncognitoReauthController
                .getIncognitoReauthCallbackForTesting()
                .onIncognitoReauthSuccess();
        verifyNoMoreInteractions(mIncognitoReauthCallbackMock);
    }

    @Test
    @SmallTest
    public void testLayoutStateChange_HidesOrShowsReauthScreen() {
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();

        // Chrome went to background.
        mIncognitoReauthController.onTaskVisibilityChanged(TASK_ID, false);
        // Chrome coming to foregrounded. Re-auth would now be required since there are existing
        // Incognito tabs.
        doReturn(true).when(mTabModelSelectorMock).isIncognitoBrandedModelSelected();
        mIncognitoReauthController.onStartWithNative();
        assertTrue(mIncognitoReauthController.isReauthPageShowing());

        // Trigger layout state change to indicate tab switcher is hidden.
        mLayoutStateObserverArgumentCaptor.getValue().onFinishedHiding(LayoutType.TAB_SWITCHER);
        assertFalse(
                "Re-auth screen shouldn't be shown if we came out of tab switcher.",
                mIncognitoReauthController.isReauthPageShowing());

        // Trigger layout state change to indicate we are now showing a tab.
        mLayoutStateObserverArgumentCaptor.getValue().onStartedShowing(LayoutType.BROWSING);
        assertTrue(
                "Re-auth screen should be shown if we are about to show a tab.",
                mIncognitoReauthController.isReauthPageShowing());
    }
}
