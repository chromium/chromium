// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.tabswitcher;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabModelDotInfo;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.OverridableTabCount;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.button.ButtonState;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.VersioningMessageController;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;

/** Unit tests for {@link TabSwitcherActionProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.DATA_SHARING})
public class TabSwitcherActionProviderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UserPrefsJni mMockUserPrefsJni;
    @Mock private PrefService mPrefService;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private VersioningMessageController mVersioningMessageController;

    @Mock private ActionRegistry mActionRegistry;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Mock private OverridableTabCount mOverridableTabCount;
    private SettableNonNullObservableSupplier<Integer> mTabCountSupplier;
    private SettableNonNullObservableSupplier<TabModelDotInfo> mNotificationDotSupplier;
    @Mock private OneshotSupplier<Boolean> mPromoShownOneshotSupplier;
    private SettableNonNullObservableSupplier<Integer> mArchivedTabCountSupplier;
    @Mock private OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    @Mock private Runnable mOnTabSwitcherClicked;
    @Mock private Callback<View> mOnTabSwitcherLongClicked;
    @Mock private Runnable mArchivedTabsIphShownCallback;
    @Mock private Runnable mArchivedTabsIphDismissedCallback;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private PropertyObservable.PropertyObserver<PropertyKey> mPropertyObserver;

    @Captor private ArgumentCaptor<PropertyModel> mModelCaptor;
    @Captor private ArgumentCaptor<LayoutStateObserver> mLayoutStateObserverCaptor;

    @Captor
    private ArgumentCaptor<Callback<LayoutStateProvider>> mLayoutStateProviderCallbackCaptor;

    private TabSwitcherActionProvider mProvider;
    private PropertyModel mModel;

    @Before
    public void setUp() {

        mTabCountSupplier = ObservableSuppliers.createNonNull(1);
        mNotificationDotSupplier =
                ObservableSuppliers.createNonNull(new TabModelDotInfo(false, null));
        mArchivedTabCountSupplier = ObservableSuppliers.createNonNull(0);

        when(mOverridableTabCount.getObservable()).thenReturn(mTabCountSupplier);
        when(mIncognitoStateProvider.isIncognitoSelected()).thenReturn(false);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.getCurrentTabSupplier())
                .thenReturn(ObservableSuppliers.alwaysNull());

        UserPrefsJni.setInstanceForTesting(mMockUserPrefsJni);
        when(mMockUserPrefsJni.get(any())).thenReturn(mPrefService);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getVersioningMessageController())
                .thenReturn(mVersioningMessageController);
        when(mVersioningMessageController.isInitialized()).thenReturn(false);

        mProvider =
                new TabSwitcherActionProvider(
                        mActionRegistry,
                        mUserEducationHelper,
                        mTabModelSelector,
                        mIncognitoStateProvider,
                        mOverridableTabCount,
                        mNotificationDotSupplier,
                        mPromoShownOneshotSupplier,
                        mArchivedTabCountSupplier,
                        mLayoutStateProviderSupplier,
                        mOnTabSwitcherClicked,
                        mOnTabSwitcherLongClicked,
                        mArchivedTabsIphShownCallback,
                        mArchivedTabsIphDismissedCallback);

        // Verify registration and capture model
        verify(mActionRegistry).register(eq(ActionId.TAB_SWITCHER), mModelCaptor.capture());
        mModel = mModelCaptor.getValue();

        // Capture LayoutStateProvider callback
        verify(mLayoutStateProviderSupplier)
                .onAvailable(mLayoutStateProviderCallbackCaptor.capture());
        // Provide the mock LayoutStateProvider
        mLayoutStateProviderCallbackCaptor.getValue().onResult(mLayoutStateProvider);
        // Capture LayoutStateObserver
        verify(mLayoutStateProvider).addObserver(mLayoutStateObserverCaptor.capture());
    }

    @After
    public void tearDown() {
        UserPrefsJni.setInstanceForTesting(null);
        TabGroupSyncServiceFactory.setForTesting(null);
    }

    @Test
    public void testTriggerEmittedOnTabSwitcherShow() {
        LayoutStateObserver observer = mLayoutStateObserverCaptor.getValue();

        mModel.addObserver(mPropertyObserver);

        // Trigger show
        observer.onStartedShowing(LayoutType.TAB_SWITCHER);

        // Verify trigger was emitted
        verify(mPropertyObserver)
                .onPropertyChanged(
                        eq(mModel), eq(TabSwitcherActionProperties.SHOW_TAB_SWITCHER_TRIGGER));

        // Trigger show again
        observer.onStartedShowing(LayoutType.TAB_SWITCHER);

        // Verify trigger emitted again
        verify(mPropertyObserver, times(2))
                .onPropertyChanged(
                        eq(mModel), eq(TabSwitcherActionProperties.SHOW_TAB_SWITCHER_TRIGGER));
    }

    @Test
    public void testIncognitoStateChange() {
        ArgumentCaptor<IncognitoStateProvider.IncognitoStateObserver> observerCaptor =
                ArgumentCaptor.forClass(IncognitoStateProvider.IncognitoStateObserver.class);
        verify(mIncognitoStateProvider)
                .addIncognitoStateObserverAndTrigger(observerCaptor.capture());
        IncognitoStateProvider.IncognitoStateObserver observer = observerCaptor.getValue();

        // Trigger incognito change
        observer.onIncognitoStateChanged(true);

        // Verify model is updated
        assertEquals(true, mModel.get(TabSwitcherActionProperties.IS_INCOGNITO));
    }

    @Test
    public void testTabCountUpdate_UnclickableWhenZero() {
        // Update tab count to 0
        mTabCountSupplier.set(0);

        // Verify button state is UNCLICKABLE
        assertEquals(ButtonState.UNCLICKABLE, mModel.get(ActionProperties.BUTTON_STATE));
    }

    @Test
    public void testTabCountUpdate_DefaultWhenGreaterThanZero() {
        // Update tab count to 5
        mTabCountSupplier.set(5);

        // Verify button state is DEFAULT
        assertEquals(ButtonState.DEFAULT, mModel.get(ActionProperties.BUTTON_STATE));
        assertEquals(5, mModel.get(TabSwitcherActionProperties.TAB_COUNT));
    }

    @Test
    public void testIphRequestedOnPageLoadFinished_DefaultIPH() {
        when(mPromoShownOneshotSupplier.get()).thenReturn(false);
        when(mIncognitoStateProvider.isIncognitoSelected()).thenReturn(false);

        // Mock profile
        Profile profile = mock(Profile.class);
        TabModel tabModel = mock(TabModel.class);
        when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);
        when(tabModel.getProfile()).thenReturn(profile);
        when(tabModel.isIncognitoBranded()).thenReturn(false);

        mProvider.handlePageLoadFinished();

        // Verify IPH requested
        assertEquals(
                FeatureConstants.TAB_SWITCHER_BUTTON_FEATURE,
                mModel.get(ActionProperties.IPH_INTENT).getFeatureNameForTesting());
    }

    @Test
    public void testIphIncognito() {
        when(mPromoShownOneshotSupplier.get()).thenReturn(false);
        when(mIncognitoStateProvider.isIncognitoSelected()).thenReturn(true);

        ArgumentCaptor<IncognitoStateProvider.IncognitoStateObserver> observerCaptor =
                ArgumentCaptor.forClass(IncognitoStateProvider.IncognitoStateObserver.class);
        verify(mIncognitoStateProvider)
                .addIncognitoStateObserverAndTrigger(observerCaptor.capture());
        observerCaptor.getValue().onIncognitoStateChanged(true);

        // Mock profile
        TabModel tabModel = mock(TabModel.class);
        when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);
        when(tabModel.isIncognitoBranded()).thenReturn(true);

        mProvider.handlePageLoadFinished();

        // Verify no IPH requested when incognito is selected
        assertNull(mModel.get(ActionProperties.IPH_INTENT));
    }

    @Test
    public void testIphShowedPromo() {
        when(mPromoShownOneshotSupplier.get()).thenReturn(true);
        when(mIncognitoStateProvider.isIncognitoSelected()).thenReturn(false);

        // Mock profile
        Profile profile = mock(Profile.class);
        TabModel tabModel = mock(TabModel.class);
        when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);
        when(tabModel.getProfile()).thenReturn(profile);
        when(tabModel.isIncognitoBranded()).thenReturn(false);

        mProvider.handlePageLoadFinished();

        // Verify IPH NOT requested for promo already shown
        assertNull(mModel.get(ActionProperties.IPH_INTENT));
    }

    @Test
    public void testTabModelDotInfoIph() {
        String groupTitle = "Vacation";
        mNotificationDotSupplier.set(new TabModelDotInfo(true, groupTitle));

        // Verify IPH requested
        assertEquals(
                FeatureConstants.TAB_GROUP_SHARE_UPDATE_FEATURE,
                mModel.get(ActionProperties.IPH_INTENT).getFeatureNameForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testSwitchToIncognitoIphIsShown() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        when(mPromoShownOneshotSupplier.get()).thenReturn(false);

        // Standard model with incognito tabs - show switch into incognito IPH.
        TabModel standardModel = mock(TabModel.class);
        TabModel incognitoModel = mock(TabModel.class);
        when(mTabModelSelector.getCurrentModel()).thenReturn(standardModel);
        when(mTabModelSelector.getModel(true)).thenReturn(incognitoModel);
        when(standardModel.isIncognitoBranded()).thenReturn(false);
        when(incognitoModel.getCount()).thenReturn(1);

        mProvider.handlePageLoadFinished();

        assertEquals(
                FeatureConstants.TAB_SWITCHER_BUTTON_SWITCH_INCOGNITO,
                mModel.get(ActionProperties.IPH_INTENT).getFeatureNameForTesting());

        // Incognito model - show switch out of incognito IPH.
        when(mTabModelSelector.getCurrentModel()).thenReturn(incognitoModel);
        when(incognitoModel.isIncognitoBranded()).thenReturn(true);

        mProvider.handlePageLoadFinished();

        assertEquals(
                FeatureConstants.TAB_SWITCHER_BUTTON_SWITCH_INCOGNITO,
                mModel.get(ActionProperties.IPH_INTENT).getFeatureNameForTesting());
    }

    @Test
    public void testIphXr() {
        DeviceInfo.setIsXrForTesting(true);

        mTabCountSupplier.set(3);

        assertEquals(
                FeatureConstants.IPH_TAB_SWITCHER_XR,
                mModel.get(ActionProperties.IPH_INTENT).getFeatureNameForTesting());
    }

    @Test
    public void testDeclutterIph() {
        mArchivedTabCountSupplier.set(1);

        assertEquals(
                FeatureConstants.ANDROID_TAB_DECLUTTER_FEATURE,
                mModel.get(ActionProperties.IPH_INTENT).getFeatureNameForTesting());
    }

    @Test
    public void testIphNotRequestedWhenTabStateNotInitialized() {
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        TabSwitcherActionProvider provider =
                new TabSwitcherActionProvider(
                        mActionRegistry,
                        mUserEducationHelper,
                        mTabModelSelector,
                        mIncognitoStateProvider,
                        mOverridableTabCount,
                        mNotificationDotSupplier,
                        mPromoShownOneshotSupplier,
                        mArchivedTabCountSupplier,
                        mLayoutStateProviderSupplier,
                        mOnTabSwitcherClicked,
                        mOnTabSwitcherLongClicked,
                        mArchivedTabsIphShownCallback,
                        mArchivedTabsIphDismissedCallback);

        provider.handlePageLoadFinished();

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mActionRegistry, times(2))
                .register(eq(ActionId.TAB_SWITCHER), modelCaptor.capture());
        PropertyModel model = modelCaptor.getAllValues().get(1);

        assertNull(model.get(ActionProperties.IPH_INTENT));
    }

    @Test
    public void testIphXr_Incognito_Triggers() {
        DeviceInfo.setIsXrForTesting(true);
        when(mIncognitoStateProvider.isIncognitoSelected()).thenReturn(true);

        ArgumentCaptor<IncognitoStateProvider.IncognitoStateObserver> observerCaptor =
                ArgumentCaptor.forClass(IncognitoStateProvider.IncognitoStateObserver.class);
        verify(mIncognitoStateProvider)
                .addIncognitoStateObserverAndTrigger(observerCaptor.capture());
        observerCaptor.getValue().onIncognitoStateChanged(true);

        mTabCountSupplier.set(3);

        assertEquals(
                FeatureConstants.IPH_TAB_SWITCHER_XR,
                mModel.get(ActionProperties.IPH_INTENT).getFeatureNameForTesting());
    }
}
