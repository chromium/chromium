// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.HUB_SEARCH_ENABLED_STATE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.MANUAL_SEARCH_BOX_ANIMATION;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.MENU_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_BUTTON_LOOKUP_CALLBACK;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_INDEX;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_VISIBILITY_FRACTION;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_LISTENER;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_LOUPE_VISIBLE;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.view.View;

import com.google.common.collect.ImmutableSet;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.hub.HubToolbarMediator.HubSearchEntrypoint;
import org.chromium.chrome.browser.hub.HubToolbarProperties.PaneButtonLookup;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient.IntentBuilder;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

import java.util.Arrays;
import java.util.List;

/** Tests for {@link HubToolbarMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubToolbarMediatorUnitTest {
    private static final int NARROW_SCREEN_WIDTH_DP = 300;
    private static final int WIDE_SCREEN_WIDTH_DP = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private Resources mResources;
    @Mock private Configuration mConfiguration;
    @Mock private PaneManager mPaneManager;
    @Mock private Pane mTabSwitcherPane;
    @Mock private Pane mIncognitoTabSwitcherPane;
    @Mock private Pane mTabGroupsPane;
    @Mock private Pane mBookmarksPane;
    @Mock private Pane mHistoryPane;
    @Mock private PaneOrderController mPaneOrderController;
    @Mock private DisplayButtonData mDisplayButtonData;
    @Mock private DisplayButtonData mDisplayButtonData2;
    @Mock private PropertyObserver<PropertyKey> mPropertyObserver;
    @Mock private View mButton1;
    @Mock private View mButton2;
    @Mock private Tracker mTracker;
    @Mock private SearchActivityClient mSearchActivityClient;
    @Mock private IntentBuilder mIntentBuilder;
    @Mock private Intent mIntent;
    @Mock private Runnable mExitHubRunnable;
    @Mock private HubColorMixer mColorMixer;

    private final SettableMonotonicObservableSupplier<Pane> mFocusedPaneSupplier =
            ObservableSuppliers.createMonotonic();

    private SettableNullableObservableSupplier<DisplayButtonData>
            mTabSwitcherReferenceButtonDataSupplier1;
    private final SettableNonNullObservableSupplier<Boolean> mRegularHubSearchEnabledStateSupplier =
            ObservableSuppliers.createNonNull(true);
    private final SettableNonNullObservableSupplier<Boolean>
            mIncognitoHubSearchEnabledStateSupplier = ObservableSuppliers.createNonNull(true);
    private final SettableNonNullObservableSupplier<Boolean>
            mTabSwitcherSearchBoxVisibilitySupplier = ObservableSuppliers.createNonNull(true);
    private SettableMonotonicObservableSupplier<DisplayButtonData>
            mIncognitoTabSwitcherReferenceButtonDataSupplier2;
    private final SettableNonNullObservableSupplier<Boolean> mManualSearchBoxAnimationSupplier =
            ObservableSuppliers.createNonNull(false);
    private final SettableNonNullObservableSupplier<Float> mSearchBoxVisibilityFractionSupplier =
            ObservableSuppliers.createNonNull(0.0f);
    private final SettableNonNullObservableSupplier<Boolean>
            mIncognitoManualSearchBoxAnimationSupplier = ObservableSuppliers.createNonNull(false);
    private final SettableNonNullObservableSupplier<Float>
            mIncognitoSearchBoxVisibilityFractionSupplier = ObservableSuppliers.createNonNull(0.0f);
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mTabSwitcherReferenceButtonDataSupplier1 =
                ObservableSuppliers.createNullable(mDisplayButtonData);
        mIncognitoTabSwitcherReferenceButtonDataSupplier2 =
                ObservableSuppliers.createMonotonic(mDisplayButtonData);
        mModel =
                new PropertyModel.Builder(HubToolbarProperties.ALL_KEYS)
                        .with(COLOR_MIXER, mColorMixer)
                        .build();
        mModel.addObserver(mPropertyObserver);

        when(mPaneOrderController.getPaneOrder())
                .thenReturn(
                        ImmutableSet.of(
                                PaneId.TAB_SWITCHER,
                                PaneId.INCOGNITO_TAB_SWITCHER,
                                PaneId.CROSS_DEVICE));
        when(mPaneManager.getFocusedPaneSupplier()).thenReturn(mFocusedPaneSupplier);
        when(mPaneManager.getPaneOrderController()).thenReturn(mPaneOrderController);
        when(mPaneManager.getPaneForId(PaneId.TAB_SWITCHER)).thenReturn(mTabSwitcherPane);
        when(mPaneManager.getPaneForId(PaneId.INCOGNITO_TAB_SWITCHER))
                .thenReturn(mIncognitoTabSwitcherPane);
        when(mPaneManager.getPaneForId(PaneId.TAB_GROUPS)).thenReturn(mTabGroupsPane);
        when(mPaneManager.getPaneForId(PaneId.BOOKMARKS)).thenReturn(mBookmarksPane);
        when(mPaneManager.getPaneForId(PaneId.HISTORY)).thenReturn(mHistoryPane);

        when(mTabSwitcherPane.getHubSearchEnabledStateSupplier())
                .thenReturn(mRegularHubSearchEnabledStateSupplier);
        when(mTabSwitcherPane.getHubSearchBoxVisibilitySupplier())
                .thenReturn(mTabSwitcherSearchBoxVisibilitySupplier);
        when(mIncognitoTabSwitcherPane.getHubSearchBoxVisibilitySupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(mTabGroupsPane.getHubSearchBoxVisibilitySupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(mBookmarksPane.getHubSearchBoxVisibilitySupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(mHistoryPane.getHubSearchBoxVisibilitySupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(mIncognitoTabSwitcherPane.getHubSearchEnabledStateSupplier())
                .thenReturn(mIncognitoHubSearchEnabledStateSupplier);
        when(mTabGroupsPane.getHubSearchEnabledStateSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(mHistoryPane.getHubSearchEnabledStateSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        when(mTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mTabSwitcherReferenceButtonDataSupplier1);
        when(mTabSwitcherPane.getActionButtonDataSupplier())
                .thenReturn(ObservableSuppliers.alwaysNull());
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        when(mTabSwitcherPane.getColorScheme()).thenReturn(HubColorScheme.DEFAULT);

        when(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mIncognitoTabSwitcherReferenceButtonDataSupplier2);
        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        when(mIncognitoTabSwitcherPane.getColorScheme()).thenReturn(HubColorScheme.INCOGNITO);

        when(mTabSwitcherPane.getManualSearchBoxAnimationSupplier())
                .thenReturn(mManualSearchBoxAnimationSupplier);
        when(mTabSwitcherPane.getSearchBoxVisibilityFractionSupplier())
                .thenReturn(mSearchBoxVisibilityFractionSupplier);
        when(mIncognitoTabSwitcherPane.getManualSearchBoxAnimationSupplier())
                .thenReturn(mIncognitoManualSearchBoxAnimationSupplier);
        when(mIncognitoTabSwitcherPane.getSearchBoxVisibilityFractionSupplier())
                .thenReturn(mIncognitoSearchBoxVisibilityFractionSupplier);
        when(mTabGroupsPane.getManualSearchBoxAnimationSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(mTabGroupsPane.getSearchBoxVisibilityFractionSupplier())
                .thenReturn(ObservableSuppliers.createNonNull(0.0f));
        when(mBookmarksPane.getManualSearchBoxAnimationSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(mBookmarksPane.getSearchBoxVisibilityFractionSupplier())
                .thenReturn(ObservableSuppliers.createNonNull(0.0f));
        when(mHistoryPane.getManualSearchBoxAnimationSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(mHistoryPane.getSearchBoxVisibilityFractionSupplier())
                .thenReturn(ObservableSuppliers.createNonNull(0.0f));

        when(mTabGroupsPane.getPaneId()).thenReturn(PaneId.TAB_GROUPS);
        when(mHistoryPane.getPaneId()).thenReturn(PaneId.HISTORY);

        mConfiguration.screenWidthDp = NARROW_SCREEN_WIDTH_DP;
        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getConfiguration()).thenReturn(mConfiguration);
    }

    @Test
    public void testDestroy() {
        assertFalse(mFocusedPaneSupplier.hasObservers());

        HubToolbarMediator mediator =
                new HubToolbarMediator(
                        mActivity,
                        mModel,
                        mPaneManager,
                        mTracker,
                        mSearchActivityClient,
                        mExitHubRunnable);
        assertTrue(mFocusedPaneSupplier.hasObservers());

        mediator.destroy();
        assertFalse(mFocusedPaneSupplier.hasObservers());

        assertFalse(mTabSwitcherReferenceButtonDataSupplier1.hasObservers());
        assertFalse(mIncognitoTabSwitcherReferenceButtonDataSupplier2.hasObservers());
    }

    @Test
    public void testHubSearchEnabledStateSupplier() {
        assertFalse(mRegularHubSearchEnabledStateSupplier.hasObservers());

        when(mPaneOrderController.getPaneOrder())
                .thenReturn(ImmutableSet.of(PaneId.TAB_SWITCHER, PaneId.INCOGNITO_TAB_SWITCHER));
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        HubToolbarMediator mediator =
                new HubToolbarMediator(
                        mActivity,
                        mModel,
                        mPaneManager,
                        mTracker,
                        mSearchActivityClient,
                        mExitHubRunnable);
        assertTrue(mRegularHubSearchEnabledStateSupplier.hasObservers());

        mRegularHubSearchEnabledStateSupplier.set(false);
        assertFalse(mModel.get(HUB_SEARCH_ENABLED_STATE));
        mRegularHubSearchEnabledStateSupplier.set(true);
        assertTrue(mModel.get(HUB_SEARCH_ENABLED_STATE));

        mediator.destroy();
        assertFalse(mRegularHubSearchEnabledStateSupplier.hasObservers());
    }

    @Test
    public void testHubSearchEnabledStateSupplier_TogglePanesIncognitoReauth() {
        when(mPaneOrderController.getPaneOrder())
                .thenReturn(ImmutableSet.of(PaneId.TAB_SWITCHER, PaneId.INCOGNITO_TAB_SWITCHER));
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(
                mActivity,
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);

        // Mimic incognito reauth pending
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertTrue(mModel.get(HUB_SEARCH_ENABLED_STATE));

        // Toggle panes back to the tab switcher
        mIncognitoHubSearchEnabledStateSupplier.set(false);
        assertFalse(mModel.get(HUB_SEARCH_ENABLED_STATE));
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        assertTrue(mModel.get(HUB_SEARCH_ENABLED_STATE));

        // Toggle panes back to the incognito tab switcher.
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertFalse(mModel.get(HUB_SEARCH_ENABLED_STATE));
    }

    @Test
    public void testPaneSwitcherButtonData() {
        new HubToolbarMediator(
                mActivity,
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);
        List<FullButtonData> paneSwitcherButtonData = mModel.get(PANE_SWITCHER_BUTTON_DATA);
        assertEquals(2, paneSwitcherButtonData.size());

        paneSwitcherButtonData.get(1).getOnPressRunnable().run();
        verify(mPaneManager).focusPane(PaneId.INCOGNITO_TAB_SWITCHER);

        paneSwitcherButtonData.get(0).getOnPressRunnable().run();
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
    }

    @Test
    public void testNullPane() {
        when(mPaneManager.getPaneForId(PaneId.INCOGNITO_TAB_SWITCHER)).thenReturn(null);

        new HubToolbarMediator(
                mActivity,
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);
        List<FullButtonData> paneSwitcherButtonData = mModel.get(PANE_SWITCHER_BUTTON_DATA);
        assertEquals(1, paneSwitcherButtonData.size());

        paneSwitcherButtonData.get(0).getOnPressRunnable().run();
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
    }

    @Test
    public void testPaneSwitcherButtonDataEventCount() {
        verify(mPropertyObserver, never()).onPropertyChanged(any(), eq(PANE_SWITCHER_BUTTON_DATA));

        new HubToolbarMediator(
                mActivity,
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);
        verify(mPropertyObserver, times(1)).onPropertyChanged(any(), eq(PANE_SWITCHER_BUTTON_DATA));

        mTabSwitcherReferenceButtonDataSupplier1.set(mDisplayButtonData);
        verify(mPropertyObserver, times(1)).onPropertyChanged(any(), eq(PANE_SWITCHER_BUTTON_DATA));

        mTabSwitcherReferenceButtonDataSupplier1.set(null);
        verify(mPropertyObserver, times(2)).onPropertyChanged(any(), eq(PANE_SWITCHER_BUTTON_DATA));

        mTabSwitcherReferenceButtonDataSupplier1.set(mDisplayButtonData);
        verify(mPropertyObserver, times(3)).onPropertyChanged(any(), eq(PANE_SWITCHER_BUTTON_DATA));
    }

    @Test
    public void testPaneSwitcherIndex() {
        new HubToolbarMediator(
                mActivity,
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);
        assertEquals(-1, mModel.get(PANE_SWITCHER_INDEX));

        mFocusedPaneSupplier.set(mTabSwitcherPane);
        assertEquals(0, mModel.get(PANE_SWITCHER_INDEX));

        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertEquals(1, mModel.get(PANE_SWITCHER_INDEX));

        mFocusedPaneSupplier.set(mTabSwitcherPane);
        mTabSwitcherReferenceButtonDataSupplier1.set(null);
        assertEquals(-1, mModel.get(PANE_SWITCHER_INDEX));

        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertEquals(0, mModel.get(PANE_SWITCHER_INDEX));

        mTabSwitcherReferenceButtonDataSupplier1.set(mDisplayButtonData);
        assertEquals(1, mModel.get(PANE_SWITCHER_INDEX));

        mFocusedPaneSupplier.set(mTabSwitcherPane);
        assertEquals(0, mModel.get(PANE_SWITCHER_INDEX));
    }

    @Test
    public void testMenuButtonVisibility() {
        new HubToolbarMediator(
                mActivity,
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);
        when(mIncognitoTabSwitcherPane.getMenuButtonVisible()).thenReturn(false);
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertFalse(mModel.get(MENU_BUTTON_VISIBLE));

        when(mTabSwitcherPane.getMenuButtonVisible()).thenReturn(true);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        assertTrue(mModel.get(MENU_BUTTON_VISIBLE));
    }

    @Test
    public void testPaneButtonLookupCallback() {
        HubToolbarMediator mediator =
                new HubToolbarMediator(
                        mActivity,
                        mModel,
                        mPaneManager,
                        mTracker,
                        mSearchActivityClient,
                        mExitHubRunnable);
        assertEquals(2, mModel.get(PANE_SWITCHER_BUTTON_DATA).size());
        assertNull(mediator.getButton(PaneId.TAB_SWITCHER));

        Callback<PaneButtonLookup> paneButtonLookupCallback =
                mModel.get(PANE_BUTTON_LOOKUP_CALLBACK);
        assertNotNull(paneButtonLookupCallback);
        paneButtonLookupCallback.onResult(Arrays.asList(mButton1, mButton2)::get);

        assertEquals(mButton1, mediator.getButton(PaneId.TAB_SWITCHER));
        assertEquals(mButton2, mediator.getButton(PaneId.INCOGNITO_TAB_SWITCHER));
        assertEquals(null, mediator.getButton(PaneId.BOOKMARKS));
        assertEquals(null, mediator.getButton(PaneId.TAB_GROUPS));
    }

    @Test
    public void testTrackerNotifyEvent() {
        when(mPaneOrderController.getPaneOrder())
                .thenReturn(ImmutableSet.of(PaneId.TAB_SWITCHER, PaneId.TAB_GROUPS));
        when(mPaneManager.getPaneForId(PaneId.TAB_GROUPS)).thenReturn(mTabSwitcherPane);

        new HubToolbarMediator(
                mActivity,
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);
        List<FullButtonData> paneSwitcherButtonData = mModel.get(PANE_SWITCHER_BUTTON_DATA);
        assertEquals(2, paneSwitcherButtonData.size());

        paneSwitcherButtonData.get(0).getOnPressRunnable().run();
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
        verifyNoInteractions(mTracker);

        paneSwitcherButtonData.get(1).getOnPressRunnable().run();
        verify(mPaneManager).focusPane(PaneId.TAB_GROUPS);
        verify(mTracker).notifyEvent("tab_groups_surface_clicked");
    }

    @Test
    public void testIsCurrentPaneIncognito() {
        new HubToolbarMediator(
                mActivity,
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        assertFalse(mModel.get(IS_INCOGNITO));
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertTrue(mModel.get(IS_INCOGNITO));
    }

    @Test
    public void testSearchBoxSetup() {
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(
                Robolectric.buildActivity(Activity.class).get(),
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);
        assertTrue(mModel.get(SEARCH_BOX_VISIBLE));
        assertFalse(mModel.get(SEARCH_LOUPE_VISIBLE));
        assertNotNull(mModel.get(SEARCH_LISTENER));
    }

    @Test
    public void testSearchBoxSetup_Tablet() {
        mConfiguration.screenWidthDp = WIDE_SCREEN_WIDTH_DP;

        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(
                mActivity,
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));
        assertTrue(mModel.get(SEARCH_LOUPE_VISIBLE));
        assertNotNull(mModel.get(SEARCH_LISTENER));
    }

    @Test
    public void testSearchBox_TogglePanesSearchBoxVisibility_Phone() {
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(
                Robolectric.buildActivity(Activity.class).get(),
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);
        assertFalse(mModel.get(APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION));
        assertTrue(mModel.get(SEARCH_BOX_VISIBLE));
        assertFalse(mModel.get(SEARCH_LOUPE_VISIBLE));

        when(mTabGroupsPane.getPaneId()).thenReturn(PaneId.BOOKMARKS);
        mFocusedPaneSupplier.set(mTabGroupsPane);
        assertTrue(mModel.get(APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION));
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));
        assertFalse(mModel.get(SEARCH_LOUPE_VISIBLE));

        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertFalse(mModel.get(APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION));
        assertTrue(mModel.get(SEARCH_BOX_VISIBLE));
        assertFalse(mModel.get(SEARCH_LOUPE_VISIBLE));
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS)
    public void testSearchBoxToGroups_TogglePanesSearchBoxVisibility_Phone() {
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(
                Robolectric.buildActivity(Activity.class).get(),
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);
        assertFalse(mModel.get(APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION));
        assertTrue(mModel.get(SEARCH_BOX_VISIBLE));
        assertFalse(mModel.get(SEARCH_LOUPE_VISIBLE));

        when(mTabGroupsPane.getPaneId()).thenReturn(PaneId.TAB_GROUPS);
        mFocusedPaneSupplier.set(mTabGroupsPane);
        assertFalse(mModel.get(APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION));
        assertTrue(mModel.get(SEARCH_BOX_VISIBLE));
        assertFalse(mModel.get(SEARCH_LOUPE_VISIBLE));

        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertFalse(mModel.get(APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION));
        assertTrue(mModel.get(SEARCH_BOX_VISIBLE));
        assertFalse(mModel.get(SEARCH_LOUPE_VISIBLE));
    }

    @Test
    public void testSearchBox_TogglePanesSearchBoxVisibility_Tablet() {
        mConfiguration.screenWidthDp = WIDE_SCREEN_WIDTH_DP;

        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(
                mActivity,
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));
        assertTrue(mModel.get(SEARCH_LOUPE_VISIBLE));

        when(mTabGroupsPane.getPaneId()).thenReturn(PaneId.BOOKMARKS);
        mFocusedPaneSupplier.set(mTabGroupsPane);
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));
        assertFalse(mModel.get(SEARCH_LOUPE_VISIBLE));

        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));
        assertTrue(mModel.get(SEARCH_LOUPE_VISIBLE));
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS)
    public void testSearchBoxToGroups_TogglePanesSearchBoxVisibility_Tablet() {
        mConfiguration.screenWidthDp = WIDE_SCREEN_WIDTH_DP;

        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(
                mActivity,
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));
        assertTrue(mModel.get(SEARCH_LOUPE_VISIBLE));

        when(mTabGroupsPane.getPaneId()).thenReturn(PaneId.TAB_GROUPS);
        mFocusedPaneSupplier.set(mTabGroupsPane);
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));
        assertTrue(mModel.get(SEARCH_LOUPE_VISIBLE));

        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));
        assertTrue(mModel.get(SEARCH_LOUPE_VISIBLE));
    }

    @Test
    public void testSearchBox_ClickListener_Phone() {
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(
                Robolectric.buildActivity(Activity.class).get(),
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);
        assertTrue(mModel.get(SEARCH_BOX_VISIBLE));
        assertFalse(mModel.get(SEARCH_LOUPE_VISIBLE));
        assertNotNull(mModel.get(SEARCH_LISTENER));

        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.HubSearch.SearchBoxEntrypointV2",
                                HubSearchEntrypoint.REGULAR_SEARCHBOX)
                        .expectIntRecord(
                                "Android.HubSearch.SearchBoxEntrypointV2",
                                HubSearchEntrypoint.INCOGNITO_SEARCHBOX)
                        .expectIntRecord(
                                "Android.HubSearch.SearchBoxEntrypointV2",
                                HubSearchEntrypoint.TAB_GROUPS_SEARCHBOX)
                        .build();

        // Fake clicks on the search box.
        mockSearchActivityClient();
        mModel.get(SEARCH_LISTENER).run();
        verify(mSearchActivityClient).requestOmniboxForResult(any());

        // Toggle to incognito pane
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        mModel.get(SEARCH_LISTENER).run();
        verify(mSearchActivityClient, times(2)).requestOmniboxForResult(any());

        // Toggle to tab groups pane
        mFocusedPaneSupplier.set(mTabGroupsPane);
        mModel.get(SEARCH_LISTENER).run();
        verify(mSearchActivityClient, times(3)).requestOmniboxForResult(any());

        // Toggle to history pane, which is not enabled for hub search
        mFocusedPaneSupplier.set(mHistoryPane);
        mModel.get(SEARCH_LISTENER).run();
        verify(mSearchActivityClient, times(3)).requestOmniboxForResult(any());
        histograms.assertExpected();
    }

    @Test
    public void testSearchBox_ClickListener_Tablet() {
        mConfiguration.screenWidthDp = WIDE_SCREEN_WIDTH_DP;
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(
                mActivity,
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));
        assertTrue(mModel.get(SEARCH_LOUPE_VISIBLE));
        assertNotNull(mModel.get(SEARCH_LISTENER));

        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.HubSearch.SearchBoxEntrypointV2",
                                HubSearchEntrypoint.REGULAR_LOUPE)
                        .expectIntRecord(
                                "Android.HubSearch.SearchBoxEntrypointV2",
                                HubSearchEntrypoint.INCOGNITO_LOUPE)
                        .expectIntRecord(
                                "Android.HubSearch.SearchBoxEntrypointV2",
                                HubSearchEntrypoint.TAB_GROUPS_LOUPE)
                        .build();

        // Fake clicks on the search box.
        mockSearchActivityClient();
        mModel.get(SEARCH_LISTENER).run();
        verify(mSearchActivityClient).requestOmniboxForResult(any());

        // Toggle to incognito pane
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        mModel.get(SEARCH_LISTENER).run();
        verify(mSearchActivityClient, times(2)).requestOmniboxForResult(any());

        // Toggle to tab groups pane
        mFocusedPaneSupplier.set(mTabGroupsPane);
        mModel.get(SEARCH_LISTENER).run();
        verify(mSearchActivityClient, times(3)).requestOmniboxForResult(any());

        // Toggle to history pane, which is not enabled for hub search
        mFocusedPaneSupplier.set(mHistoryPane);
        mModel.get(SEARCH_LISTENER).run();
        verify(mSearchActivityClient, times(3)).requestOmniboxForResult(any());
        histograms.assertExpected();
    }

    @Test
    public void testSearchBox_UsesConfigurationParameterNotContext() {
        // Set up a scenario where context configuration and parameter configuration differ
        // to verify that the parameter configuration is used

        // Context configuration shows narrow screen (phone)
        mConfiguration.screenWidthDp = NARROW_SCREEN_WIDTH_DP;

        // Set up mediator with phone configuration
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        HubToolbarMediator mediator =
                new HubToolbarMediator(
                        mActivity,
                        mModel,
                        mPaneManager,
                        mTracker,
                        mSearchActivityClient,
                        mExitHubRunnable);

        // Initially should show search box (phone behavior)
        assertTrue(mModel.get(SEARCH_BOX_VISIBLE));
        assertFalse(mModel.get(SEARCH_LOUPE_VISIBLE));

        // Create a configuration parameter with wide screen (tablet)
        Configuration tabletConfig = new Configuration();
        tabletConfig.screenWidthDp = WIDE_SCREEN_WIDTH_DP;
        tabletConfig.orientation = Configuration.ORIENTATION_LANDSCAPE;

        // Simulate configuration change with tablet config parameter
        // while context still has phone config
        mediator.triggerConfigurationChangeForTesting(tabletConfig);

        // Should now show loupe (tablet behavior) - proving it uses parameter config
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));
        assertTrue(mModel.get(SEARCH_LOUPE_VISIBLE));

        // Create another configuration parameter with narrow screen (phone)
        Configuration phoneConfig = new Configuration();
        phoneConfig.screenWidthDp = NARROW_SCREEN_WIDTH_DP;
        phoneConfig.orientation = Configuration.ORIENTATION_PORTRAIT;

        // Simulate configuration change back to phone config
        mediator.triggerConfigurationChangeForTesting(phoneConfig);

        // Should now show search box (phone behavior)
        assertTrue(mModel.get(SEARCH_BOX_VISIBLE));
        assertFalse(mModel.get(SEARCH_LOUPE_VISIBLE));
    }

    @Test
    public void testSearchBoxVisibilitySupplier_PaneSupplier() {
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        HubToolbarMediator mediator =
                new HubToolbarMediator(
                        Robolectric.buildActivity(Activity.class).get(),
                        mModel,
                        mPaneManager,
                        mTracker,
                        mSearchActivityClient,
                        mExitHubRunnable);
        assertTrue(mModel.get(SEARCH_BOX_VISIBLE));

        // Setting supplier to false should hide the search box.
        mTabSwitcherSearchBoxVisibilitySupplier.set(false);
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));

        // Setting supplier to true should show it again.
        mTabSwitcherSearchBoxVisibilitySupplier.set(true);
        assertTrue(mModel.get(SEARCH_BOX_VISIBLE));

        // On tablet, search box should remain hidden regardless of supplier.
        mConfiguration.screenWidthDp = WIDE_SCREEN_WIDTH_DP;
        mediator.triggerConfigurationChangeForTesting(mConfiguration);
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));

        mTabSwitcherSearchBoxVisibilitySupplier.set(false);
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));
    }

    @Test
    public void testButtonRunnablesAreIgnoredWhenSettingButtonData() {
        new HubToolbarMediator(
                Robolectric.buildActivity(Activity.class).get(),
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient,
                mExitHubRunnable);

        // Verify that focus pane is called when runnables are invoked.
        mModel.get(PANE_SWITCHER_BUTTON_DATA).get(0).getOnPressRunnable().run();
        verify(mPaneManager, times(1)).focusPane(anyInt());

        // Create similar logic to what the view does, where setting button data will trigger a
        // selected tab layout change, and invoke the runnable.
        boolean[] buttonDataChanged = {false};
        mModel.addObserver(
                (source, propertyKey) -> {
                    if (propertyKey == PANE_SWITCHER_BUTTON_DATA) {
                        buttonDataChanged[0] = true;
                        mModel.get(PANE_SWITCHER_BUTTON_DATA).get(0).getOnPressRunnable().run();
                    }
                });
        assertFalse(buttonDataChanged[0]);

        // Now update the button data, which should trigger runnable.
        mTabSwitcherReferenceButtonDataSupplier1.set(mDisplayButtonData2);
        assertTrue(buttonDataChanged[0]);
        // Focus requests (and histograms) should not be done a second time, because it was during
        // the button data update.
        verify(mPaneManager, times(1)).focusPane(anyInt());
    }

    @Test
    public void testManualSearchBoxAnimation() {
        HubToolbarMediator mediator =
                new HubToolbarMediator(
                        mActivity,
                        mModel,
                        mPaneManager,
                        mTracker,
                        mSearchActivityClient,
                        mExitHubRunnable);

        mFocusedPaneSupplier.set(mTabSwitcherPane);
        RobolectricUtil.runAllBackgroundAndUi();
        assertFalse(mModel.get(MANUAL_SEARCH_BOX_ANIMATION));

        mManualSearchBoxAnimationSupplier.set(true);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(mModel.get(MANUAL_SEARCH_BOX_ANIMATION));

        mManualSearchBoxAnimationSupplier.set(false);
        RobolectricUtil.runAllBackgroundAndUi();

        mediator.destroy();
        assertFalse(mManualSearchBoxAnimationSupplier.hasObservers());
    }

    @Test
    public void testSearchBoxVisibilityFraction() {
        HubToolbarMediator mediator =
                new HubToolbarMediator(
                        mActivity,
                        mModel,
                        mPaneManager,
                        mTracker,
                        mSearchActivityClient,
                        mExitHubRunnable);

        mFocusedPaneSupplier.set(mTabSwitcherPane);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(0.0f, mModel.get(SEARCH_BOX_VISIBILITY_FRACTION), 0.0f);

        mSearchBoxVisibilityFractionSupplier.set(0.5f);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(0.5f, mModel.get(SEARCH_BOX_VISIBILITY_FRACTION), 0.0f);

        mSearchBoxVisibilityFractionSupplier.set(1.0f);
        RobolectricUtil.runAllBackgroundAndUi();

        mediator.destroy();
        assertFalse(mSearchBoxVisibilityFractionSupplier.hasObservers());
    }

    @Test
    public void testPaneSwitchingForManualAnimationAndFraction() {
        new HubToolbarMediator(
                mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient, mExitHubRunnable);

        mFocusedPaneSupplier.set(mTabSwitcherPane);
        RobolectricUtil.runAllBackgroundAndUi();
        assertFalse(mModel.get(MANUAL_SEARCH_BOX_ANIMATION));
        assertEquals(0.0f, mModel.get(SEARCH_BOX_VISIBILITY_FRACTION), 0.0f);

        mManualSearchBoxAnimationSupplier.set(true);
        mSearchBoxVisibilityFractionSupplier.set(0.5f);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(mModel.get(MANUAL_SEARCH_BOX_ANIMATION));
        assertEquals(0.5f, mModel.get(SEARCH_BOX_VISIBILITY_FRACTION), 0.0f);

        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        RobolectricUtil.runAllBackgroundAndUi();
        assertFalse(mModel.get(MANUAL_SEARCH_BOX_ANIMATION));
        assertEquals(0.0f, mModel.get(SEARCH_BOX_VISIBILITY_FRACTION), 0.0f);

        mIncognitoManualSearchBoxAnimationSupplier.set(true);
        mIncognitoSearchBoxVisibilityFractionSupplier.set(0.75f);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(mModel.get(MANUAL_SEARCH_BOX_ANIMATION));
        assertEquals(0.75f, mModel.get(SEARCH_BOX_VISIBILITY_FRACTION), 0.0f);

        // Make sure we're no longer observing the other pane.
        mManualSearchBoxAnimationSupplier.set(false);
        mSearchBoxVisibilityFractionSupplier.set(0.25f);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(mModel.get(MANUAL_SEARCH_BOX_ANIMATION));
        assertEquals(0.75f, mModel.get(SEARCH_BOX_VISIBILITY_FRACTION), 0.0f);
    }

    private void mockSearchActivityClient() {
        doReturn(mIntentBuilder).when(mSearchActivityClient).newIntentBuilder();
        doReturn(mIntentBuilder).when(mIntentBuilder).setPageUrl(any());
        doReturn(mIntentBuilder).when(mIntentBuilder).setIncognito(anyBoolean());
        doReturn(mIntentBuilder).when(mIntentBuilder).setResolutionType(anyInt());
        doReturn(mIntent).when(mIntentBuilder).build();
    }
}
