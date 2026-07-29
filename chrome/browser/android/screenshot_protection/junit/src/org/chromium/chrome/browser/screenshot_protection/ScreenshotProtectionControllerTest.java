// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshot_protection;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.WindowManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.enterprise.util.DataProtectionBridge;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.policy.PolicyServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.policy.PolicyService;

/** Unit tests for {@link ScreenshotProtectionController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@SuppressWarnings("DirectInvocationOnMock")
public class ScreenshotProtectionControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private DataProtectionBridge.Natives mDataProtectionBridgeJniMock;
    @Mock private ManagedBrowserUtils.Natives mManagedBrowserUtilsJniMock;
    @Mock private PolicyService mPolicyService;

    @Captor private ArgumentCaptor<Callback<Boolean>> mScreenshotCallbackCaptor;
    @Captor private ArgumentCaptor<LayoutStateObserver> mLayoutStateObserverCaptor;
    @Captor private ArgumentCaptor<PolicyService.Observer> mPolicyServiceObserverCaptor;

    private MockTabModelSelector mTabModelSelector;
    private MockTab mNormalTab;
    private MockTab mIncognitoTab;
    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private SettableMonotonicObservableSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private SettableNullableObservableSupplier<Tab> mActivityTabProvider;
    private ScreenshotProtectionController mController;

    private boolean isWindowSecure() {
        return (mActivity.getWindow().getAttributes().flags
                        & WindowManager.LayoutParams.FLAG_SECURE)
                != 0;
    }

    @Before
    public void setUp() {
        DataProtectionBridge.setInstanceForTesting(mDataProtectionBridgeJniMock);
        ManagedBrowserUtilsJni.setInstanceForTesting(mManagedBrowserUtilsJniMock);
        PolicyServiceFactory.setPolicyServiceForTest(mPolicyService);
        mLayoutStateProviderSupplier = ObservableSuppliers.createMonotonic();
        mActivityTabProvider = ObservableSuppliers.createNullable();

        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);

        mTabModelSelector =
                new MockTabModelSelector(
                        mProfile,
                        mIncognitoProfile,
                        /* tabCount= */ 0,
                        /* incognitoTabCount= */ 0,
                        null);

        mNormalTab = mTabModelSelector.addMockTab();
        mIncognitoTab = (MockTab) mTabModelSelector.addMockIncognitoTab();

        switchActiveTab(mNormalTab, /* isIncognito= */ false);

        mActivityController = Robolectric.buildActivity(Activity.class);
        mActivity = mActivityController.setup().get();

        when(mDataProtectionBridgeJniMock.isScreenshotAllowed(any())).thenReturn(true);
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(any())).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isEnterpriseRealTimeUrlCheckModeEnabled(any()))
                .thenReturn(false);
    }

    @After
    public void tearDown() {
        if (mController != null) {
            mController.onDestroyed();
        }
        DataProtectionBridge.setInstanceForTesting(null);
        ManagedBrowserUtilsJni.setInstanceForTesting(null);
    }

    private void switchActiveTab(MockTab tab, boolean isIncognito) {
        mTabModelSelector.selectModel(isIncognito);
        TabModelUtils.selectTabById(mTabModelSelector, tab.getId(), TabSelectionType.FROM_USER);
        mActivityTabProvider.set(tab);
    }

    private void createController(boolean isCustomTab) {
        mController =
                new ScreenshotProtectionController(
                        mActivity,
                        mActivityTabProvider,
                        mTabModelSelector,
                        isCustomTab,
                        mLayoutStateProviderSupplier);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testInit_NormalTab_Allowed() {
        createController(/* isCustomTab= */ false);

        assertFalse(mController.isScreenshotBlocked());
        assertFalse(isWindowSecure());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testInit_NormalTab_BlockedByPolicy() {
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mProfile)).thenReturn(true);
        when(mDataProtectionBridgeJniMock.isScreenshotAllowed(mNormalTab)).thenReturn(false);
        createController(/* isCustomTab= */ false);

        assertTrue(mController.isScreenshotBlocked());
        assertTrue(isWindowSecure());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testInit_IncognitoTab() {
        switchActiveTab(mIncognitoTab, /* isIncognito= */ true);
        createController(/* isCustomTab= */ false);

        assertTrue(mController.isScreenshotBlocked());
        assertTrue(isWindowSecure());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testInit_IncognitoTab_FeatureEnabled() {
        switchActiveTab(mIncognitoTab, /* isIncognito= */ true);
        createController(/* isCustomTab= */ false);

        assertFalse(mController.isScreenshotBlocked());
        assertFalse(isWindowSecure());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testInit_IncognitoTab_FeatureDisabled_WhenPolicyEnabled() {
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mProfile)).thenReturn(true);
        when(mDataProtectionBridgeJniMock.isScreenshotAllowed(mIncognitoTab)).thenReturn(false);

        switchActiveTab(mIncognitoTab, /* isIncognito= */ true);
        createController(/* isCustomTab= */ false);

        assertTrue(mController.isScreenshotBlocked());
        assertTrue(isWindowSecure());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testTabChange_ToBlocked() {
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mProfile)).thenReturn(true);
        createController(/* isCustomTab= */ false);

        assertFalse(mController.isScreenshotBlocked());

        MockTab blockedTab = mTabModelSelector.addMockTab();
        when(mDataProtectionBridgeJniMock.isScreenshotAllowed(blockedTab)).thenReturn(false);

        switchActiveTab(blockedTab, /* isIncognito= */ false);

        assertTrue(mController.isScreenshotBlocked());
        assertTrue(isWindowSecure());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testTabChange_ToIncognito() {
        createController(/* isCustomTab= */ false);

        assertFalse(mController.isScreenshotBlocked());

        switchActiveTab(mIncognitoTab, /* isIncognito= */ true);

        assertTrue(mController.isScreenshotBlocked());
        assertTrue(isWindowSecure());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testPolicyChange_Subscription() {
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mProfile)).thenReturn(true);
        createController(/* isCustomTab= */ false);

        verify(mDataProtectionBridgeJniMock)
                .registerScreenshotSubscriptionCallback(
                        eq(mNormalTab), mScreenshotCallbackCaptor.capture());

        assertFalse(mController.isScreenshotBlocked());

        // Mock navigation to blocked page
        mScreenshotCallbackCaptor.getValue().onResult(false);
        assertTrue(mController.isScreenshotBlocked());
        assertTrue(isWindowSecure());

        // Mock navigation to not blocked page
        mScreenshotCallbackCaptor.getValue().onResult(true);
        assertFalse(mController.isScreenshotBlocked());
        assertFalse(isWindowSecure());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testLayoutChange_Hub() {
        createController(/* isCustomTab= */ false);

        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        verify(mLayoutStateProvider).addObserver(mLayoutStateObserverCaptor.capture());

        LayoutStateObserver observer = mLayoutStateObserverCaptor.getValue();

        observer.onStartedShowing(LayoutType.HUB);
        assertFalse(mController.isScreenshotBlocked());
        assertFalse(isWindowSecure());

        mTabModelSelector.selectModel(/* incognito= */ true);
        observer.onStartedShowing(LayoutType.HUB);
        assertTrue(mController.isScreenshotBlocked());
        assertTrue(isWindowSecure());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testLayoutChange_Hub_Enterprise() {
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mProfile)).thenReturn(true);
        createController(/* isCustomTab= */ false);

        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        verify(mLayoutStateProvider).addObserver(mLayoutStateObserverCaptor.capture());

        LayoutStateObserver observer = mLayoutStateObserverCaptor.getValue();

        when(mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)).thenReturn(true);
        observer.onStartedShowing(LayoutType.HUB);
        assertTrue(mController.isScreenshotBlocked());
        assertTrue(isWindowSecure());

        when(mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)).thenReturn(false);
        observer.onFinishedHiding(LayoutType.HUB);
        assertFalse(mController.isScreenshotBlocked());
        assertFalse(isWindowSecure());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testLayoutChange_ToolbarSwipe() {
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mProfile)).thenReturn(true);
        createController(/* isCustomTab= */ false);

        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        verify(mLayoutStateProvider).addObserver(mLayoutStateObserverCaptor.capture());

        LayoutStateObserver observer = mLayoutStateObserverCaptor.getValue();

        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TOOLBAR_SWIPE)).thenReturn(true);
        observer.onStartedShowing(LayoutType.TOOLBAR_SWIPE);
        assertTrue(mController.isScreenshotBlocked());
        assertTrue(isWindowSecure());

        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TOOLBAR_SWIPE)).thenReturn(false);
        observer.onFinishedHiding(LayoutType.TOOLBAR_SWIPE);
        assertFalse(mController.isScreenshotBlocked());
        assertFalse(isWindowSecure());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testCustomTab_NoObservation() {
        createController(/* isCustomTab= */ true);

        verify(mLayoutStateProvider, never()).addObserver(any());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testCustomTab_OnlyTabModelObservation() {
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mProfile)).thenReturn(true);
        createController(/* isCustomTab= */ true);

        verify(mDataProtectionBridgeJniMock)
                .registerScreenshotSubscriptionCallback(
                        eq(mNormalTab), mScreenshotCallbackCaptor.capture());
        verify(mLayoutStateProvider, never()).addObserver(any());

        assertFalse(mController.isScreenshotBlocked());

        // Mock navigation to blocked page
        mScreenshotCallbackCaptor.getValue().onResult(false);
        assertTrue(mController.isScreenshotBlocked());
        assertTrue(isWindowSecure());

        // Mock navigation to not blocked page
        mScreenshotCallbackCaptor.getValue().onResult(true);
        assertFalse(mController.isScreenshotBlocked());
        assertFalse(isWindowSecure());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testIncognitoCustomTab_FeatureDisabled_WhenPolicyEnabled() {
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mIncognitoProfile))
                .thenReturn(true);
        switchActiveTab(mIncognitoTab, /* isIncognito= */ true);
        when(mDataProtectionBridgeJniMock.isScreenshotAllowed(mIncognitoTab)).thenReturn(false);
        createController(/* isCustomTab= */ true);

        verify(mDataProtectionBridgeJniMock)
                .registerScreenshotSubscriptionCallback(
                        eq(mIncognitoTab), mScreenshotCallbackCaptor.capture());
        verify(mLayoutStateProvider, never()).addObserver(any());

        assertTrue(mController.isScreenshotBlocked());
    }

    @Test
    public void testDestroy() {
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mProfile)).thenReturn(true);
        createController(/* isCustomTab= */ false);

        assertTrue(mActivityTabProvider.hasObservers());

        mController.onDestroyed();

        verify(mDataProtectionBridgeJniMock).clearScreenshotSubscriptionCallback(mNormalTab);

        assertFalse(mActivityTabProvider.hasObservers());
    }

    @Test
    public void testRegisterTabObserver() {
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mProfile)).thenReturn(true);
        createController(/* isCustomTab= */ false);
        assertTrue(mActivityTabProvider.hasObservers());
    }

    @Test
    public void testTabDestroyed_ClearsCallback() {
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mProfile)).thenReturn(true);
        createController(/* isCustomTab= */ false);

        verify(mDataProtectionBridgeJniMock)
                .registerScreenshotSubscriptionCallback(eq(mNormalTab), any());

        mNormalTab.destroy();

        verify(mDataProtectionBridgeJniMock).clearScreenshotSubscriptionCallback(mNormalTab);
        assertFalse(mController.isScreenshotBlocked());
    }

    @Test
    public void testNoRegisterTabObserver() {
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mProfile)).thenReturn(false);
        when(mManagedBrowserUtilsJniMock.isEnterpriseRealTimeUrlCheckModeEnabled(mProfile))
                .thenReturn(false);
        createController(/* isCustomTab= */ false);
        assertFalse(mActivityTabProvider.hasObservers());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testPolicyChange_TurnsProtectionOn() {
        // Initially, no policy.
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mProfile)).thenReturn(false);

        createController(/* isCustomTab= */ false);

        // Verify observer was added to mPolicyService
        verify(mPolicyService, Mockito.times(2))
                .addObserver(mPolicyServiceObserverCaptor.capture());
        PolicyService.Observer observer = mPolicyServiceObserverCaptor.getValue();

        assertFalse(mController.isScreenshotBlocked());
        assertFalse(isWindowSecure());

        // Now enable policy
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mProfile)).thenReturn(true);
        when(mDataProtectionBridgeJniMock.isScreenshotAllowed(mNormalTab)).thenReturn(false);

        // Trigger policy update
        observer.onPolicyUpdated(null, null);

        // Flush Robolectric tasks to execute the posted task in onPolicyUpdated
        ShadowLooper.idleMainLooper();

        assertTrue(mController.isScreenshotBlocked());
        assertTrue(isWindowSecure());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testPolicyChange_TurnsProtectionOff() {
        // Initially, policy is enabled.
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mProfile)).thenReturn(true);
        when(mDataProtectionBridgeJniMock.isScreenshotAllowed(mNormalTab)).thenReturn(false);

        createController(/* isCustomTab= */ false);

        // Verify observer was added to mPolicyService
        verify(mPolicyService, Mockito.times(2))
                .addObserver(mPolicyServiceObserverCaptor.capture());
        PolicyService.Observer observer = mPolicyServiceObserverCaptor.getValue();

        assertTrue(mController.isScreenshotBlocked());
        assertTrue(isWindowSecure());

        // Now disable policy
        when(mDataProtectionBridgeJniMock.hasBlockingScreenshotRule(mProfile)).thenReturn(false);
        when(mDataProtectionBridgeJniMock.isScreenshotAllowed(mNormalTab)).thenReturn(true);

        // Trigger policy update
        observer.onPolicyUpdated(null, null);

        // Flush Robolectric tasks
        ShadowLooper.idleMainLooper();

        assertFalse(mController.isScreenshotBlocked());
        assertFalse(isWindowSecure());
    }
}
