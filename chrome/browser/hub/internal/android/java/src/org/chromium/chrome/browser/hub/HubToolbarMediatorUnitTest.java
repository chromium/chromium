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
import static org.chromium.chrome.browser.hub.HubToolbarProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.HUB_SEARCH_ENABLED_STATE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.MENU_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_BUTTON_LOOKUP_CALLBACK;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_INDEX;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_LISTENER;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_LOUPE_VISIBLE;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.view.View;

import androidx.test.filters.SmallTest;

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
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.hub.HubToolbarMediator.HubSearchEntrypoint;
import org.chromium.chrome.browser.hub.HubToolbarProperties.PaneButtonLookup;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient.IntentBuilder;
import org.chromium.components.feature_engagement.Tracker;
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
    @Mock private FullButtonData mFullButtonData;
    @Mock private PaneOrderController mPaneOrderController;
    @Mock private DisplayButtonData mDisplayButtonData;
    @Mock private PropertyObserver<PropertyKey> mPropertyObserver;
    @Mock private View mButton1;
    @Mock private View mButton2;
    @Mock private Tracker mTracker;
    @Mock private SearchActivityClient mSearchActivityClient;
    @Mock private IntentBuilder mIntentBuilder;
    @Mock private Intent mIntent;
    @Mock private HubColorMixer mColorMixer;

    private ObservableSupplierImpl<FullButtonData> mActionButtonSupplier;
    private ObservableSupplierImpl<Pane> mFocusedPaneSupplier;
    private ObservableSupplierImpl<DisplayButtonData> mTabSwitcherReferenceButtonDataSupplier1;
    private ObservableSupplierImpl<Boolean> mRegularHubSearchEnabledStateSupplier;
    private ObservableSupplierImpl<Boolean> mIncognitoHubSearchEnabledStateSupplier;
    private ObservableSupplierImpl<Boolean> mGroupsHubSearchEnabledStateSupplier;
    private ObservableSupplierImpl<DisplayButtonData>
            mIncognitoTabSwitcherReferenceButtonDataSupplier2;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActionButtonSupplier = new ObservableSupplierImpl<>();
        mFocusedPaneSupplier = new ObservableSupplierImpl<>();
        mTabSwitcherReferenceButtonDataSupplier1 = new ObservableSupplierImpl<>();
        mIncognitoTabSwitcherReferenceButtonDataSupplier2 = new ObservableSupplierImpl<>();
        mRegularHubSearchEnabledStateSupplier = new ObservableSupplierImpl<>();
        mIncognitoHubSearchEnabledStateSupplier = new ObservableSupplierImpl<>();
        mGroupsHubSearchEnabledStateSupplier = new ObservableSupplierImpl<>();
        mFocusedPaneSupplier = new ObservableSupplierImpl<>();
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

        when(mTabSwitcherPane.getHubSearchEnabledStateSupplier())
                .thenReturn(mRegularHubSearchEnabledStateSupplier);
        when(mIncognitoTabSwitcherPane.getHubSearchEnabledStateSupplier())
                .thenReturn(mIncognitoHubSearchEnabledStateSupplier);
        when(mTabGroupsPane.getHubSearchEnabledStateSupplier())
                .thenReturn(mGroupsHubSearchEnabledStateSupplier);

        when(mTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mTabSwitcherReferenceButtonDataSupplier1);
        when(mTabSwitcherPane.getActionButtonDataSupplier()).thenReturn(mActionButtonSupplier);
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        when(mTabSwitcherPane.getColorScheme()).thenReturn(HubColorScheme.DEFAULT);

        when(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mIncognitoTabSwitcherReferenceButtonDataSupplier2);
        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        when(mIncognitoTabSwitcherPane.getColorScheme()).thenReturn(HubColorScheme.INCOGNITO);

        mTabSwitcherReferenceButtonDataSupplier1.set(mDisplayButtonData);
        mIncognitoTabSwitcherReferenceButtonDataSupplier2.set(mDisplayButtonData);

        mConfiguration.screenWidthDp = NARROW_SCREEN_WIDTH_DP;
        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getConfiguration()).thenReturn(mConfiguration);
    }

    @Test
    @SmallTest
    public void testDestroy() {
        assertFalse(mFocusedPaneSupplier.hasObservers());

        HubToolbarMediator mediator =
                new HubToolbarMediator(
                        mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient);
        assertTrue(mFocusedPaneSupplier.hasObservers());

        mediator.destroy();
        assertFalse(mFocusedPaneSupplier.hasObservers());

        assertFalse(mTabSwitcherReferenceButtonDataSupplier1.hasObservers());
        assertFalse(mIncognitoTabSwitcherReferenceButtonDataSupplier2.hasObservers());
    }

    @Test
    @SmallTest
    public void testHubSearchEnabledStateSupplier() {
        assertFalse(mRegularHubSearchEnabledStateSupplier.hasObservers());

        when(mPaneOrderController.getPaneOrder())
                .thenReturn(ImmutableSet.of(PaneId.TAB_SWITCHER, PaneId.INCOGNITO_TAB_SWITCHER));
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        HubToolbarMediator mediator =
                new HubToolbarMediator(
                        mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient);
        assertTrue(mRegularHubSearchEnabledStateSupplier.hasObservers());

        mRegularHubSearchEnabledStateSupplier.set(false);
        assertFalse(mModel.get(HUB_SEARCH_ENABLED_STATE));
        mRegularHubSearchEnabledStateSupplier.set(true);
        assertTrue(mModel.get(HUB_SEARCH_ENABLED_STATE));

        mediator.destroy();
        assertFalse(mRegularHubSearchEnabledStateSupplier.hasObservers());
    }

    @Test
    @SmallTest
    public void testHubSearchEnabledStateSupplier_TogglePanesIncognitoReauth() {
        when(mPaneOrderController.getPaneOrder())
                .thenReturn(ImmutableSet.of(PaneId.TAB_SWITCHER, PaneId.INCOGNITO_TAB_SWITCHER));
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient);

        // Mimic incognito reauth pending
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertTrue(mModel.get(HUB_SEARCH_ENABLED_STATE));

        // Toggle panes back to the tab switcher
        mIncognitoHubSearchEnabledStateSupplier.set(false);
        assertFalse(mModel.get(HUB_SEARCH_ENABLED_STATE));
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        assertTrue(mModel.get(HUB_SEARCH_ENABLED_STATE));

        // TOggle panes back to the incognito tab switcher
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertFalse(mModel.get(HUB_SEARCH_ENABLED_STATE));
    }

    @Test
    @SmallTest
    public void testWithActionButtonData() {
        new HubToolbarMediator(mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        mActionButtonSupplier.set(mFullButtonData);
        assertEquals(mFullButtonData, mModel.get(ACTION_BUTTON_DATA));
    }

    @Test
    @SmallTest
    public void testPaneSwitcherButtonData() {
        new HubToolbarMediator(mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient);
        List<FullButtonData> paneSwitcherButtonData = mModel.get(PANE_SWITCHER_BUTTON_DATA);
        assertEquals(2, paneSwitcherButtonData.size());

        paneSwitcherButtonData.get(1).getOnPressRunnable().run();
        verify(mPaneManager).focusPane(PaneId.INCOGNITO_TAB_SWITCHER);

        paneSwitcherButtonData.get(0).getOnPressRunnable().run();
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
    }

    @Test
    @SmallTest
    public void testNullPane() {
        when(mPaneManager.getPaneForId(PaneId.INCOGNITO_TAB_SWITCHER)).thenReturn(null);

        new HubToolbarMediator(mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient);
        List<FullButtonData> paneSwitcherButtonData = mModel.get(PANE_SWITCHER_BUTTON_DATA);
        assertEquals(1, paneSwitcherButtonData.size());

        paneSwitcherButtonData.get(0).getOnPressRunnable().run();
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
    }

    @Test
    @SmallTest
    public void testPaneSwitcherButtonDataEventCount() {
        verify(mPropertyObserver, never()).onPropertyChanged(any(), eq(PANE_SWITCHER_BUTTON_DATA));

        new HubToolbarMediator(mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient);
        verify(mPropertyObserver, times(1)).onPropertyChanged(any(), eq(PANE_SWITCHER_BUTTON_DATA));

        mTabSwitcherReferenceButtonDataSupplier1.set(mDisplayButtonData);
        verify(mPropertyObserver, times(1)).onPropertyChanged(any(), eq(PANE_SWITCHER_BUTTON_DATA));

        mTabSwitcherReferenceButtonDataSupplier1.set(null);
        verify(mPropertyObserver, times(2)).onPropertyChanged(any(), eq(PANE_SWITCHER_BUTTON_DATA));

        mTabSwitcherReferenceButtonDataSupplier1.set(mDisplayButtonData);
        verify(mPropertyObserver, times(3)).onPropertyChanged(any(), eq(PANE_SWITCHER_BUTTON_DATA));
    }

    @Test
    @SmallTest
    public void testPaneSwitcherIndex() {
        new HubToolbarMediator(mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient);
        assertEquals(-1, mModel.get(PANE_SWITCHER_INDEX));

        mFocusedPaneSupplier.set(mTabSwitcherPane);
        assertEquals(0, mModel.get(PANE_SWITCHER_INDEX));

        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertEquals(1, mModel.get(PANE_SWITCHER_INDEX));

        mFocusedPaneSupplier.set(null);
        assertEquals(-1, mModel.get(PANE_SWITCHER_INDEX));

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
    @SmallTest
    public void testMenuButtonVisibility() {
        new HubToolbarMediator(mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient);
        when(mIncognitoTabSwitcherPane.getMenuButtonVisible()).thenReturn(false);
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertFalse(mModel.get(MENU_BUTTON_VISIBLE));

        when(mTabSwitcherPane.getMenuButtonVisible()).thenReturn(true);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        assertTrue(mModel.get(MENU_BUTTON_VISIBLE));

        mFocusedPaneSupplier.set(null);
        assertFalse(mModel.get(MENU_BUTTON_VISIBLE));
    }

    @Test
    @SmallTest
    public void testPaneButtonLookupCallback() {
        HubToolbarMediator mediator =
                new HubToolbarMediator(
                        mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient);
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
    @SmallTest
    public void testTrackerNotifyEvent() {
        when(mPaneOrderController.getPaneOrder())
                .thenReturn(ImmutableSet.of(PaneId.TAB_SWITCHER, PaneId.TAB_GROUPS));
        when(mPaneManager.getPaneForId(PaneId.TAB_GROUPS)).thenReturn(mTabSwitcherPane);

        new HubToolbarMediator(mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient);
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
    @SmallTest
    public void testIsCurrentPaneIncognito() {
        new HubToolbarMediator(mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        assertFalse(mModel.get(IS_INCOGNITO));
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertTrue(mModel.get(IS_INCOGNITO));
        mFocusedPaneSupplier.set(null);
        assertFalse(mModel.get(IS_INCOGNITO));
    }

    @Test
    @SmallTest
    public void testSearchBoxSetup() {
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(
                Robolectric.buildActivity(Activity.class).get(),
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient);
        assertTrue(mModel.get(SEARCH_BOX_VISIBLE));
        assertFalse(mModel.get(SEARCH_LOUPE_VISIBLE));
        assertNotNull(mModel.get(SEARCH_LISTENER));
    }

    @Test
    @SmallTest
    public void testSearchBoxSetup_Tablet() {
        mConfiguration.screenWidthDp = WIDE_SCREEN_WIDTH_DP;

        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient);
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));
        assertTrue(mModel.get(SEARCH_LOUPE_VISIBLE));
        assertNotNull(mModel.get(SEARCH_LISTENER));
    }

    @Test
    @SmallTest
    public void testSearchBox_TogglePanesSearchBoxVisibility_Phone() {
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(
                Robolectric.buildActivity(Activity.class).get(),
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient);
        assertFalse(mModel.get(APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION));
        assertTrue(mModel.get(SEARCH_BOX_VISIBLE));
        assertFalse(mModel.get(SEARCH_LOUPE_VISIBLE));

        when(mTabGroupsPane.getPaneId()).thenReturn(PaneId.TAB_GROUPS);
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
    @SmallTest
    public void testSearchBox_TogglePanesSearchBoxVisibility_Tablet() {
        mConfiguration.screenWidthDp = WIDE_SCREEN_WIDTH_DP;

        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient);
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));
        assertTrue(mModel.get(SEARCH_LOUPE_VISIBLE));

        when(mTabGroupsPane.getPaneId()).thenReturn(PaneId.TAB_GROUPS);
        mFocusedPaneSupplier.set(mTabGroupsPane);
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));
        assertFalse(mModel.get(SEARCH_LOUPE_VISIBLE));

        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertFalse(mModel.get(SEARCH_BOX_VISIBLE));
        assertTrue(mModel.get(SEARCH_LOUPE_VISIBLE));
    }

    @Test
    @SmallTest
    public void testSearchBox_ClickListener_Phone() {
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(
                Robolectric.buildActivity(Activity.class).get(),
                mModel,
                mPaneManager,
                mTracker,
                mSearchActivityClient);
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
                        .build();

        // Fake clicks on the search box.
        mockSearchActivityClient();
        mModel.get(SEARCH_LISTENER).run();
        verify(mSearchActivityClient).requestOmniboxForResult(any());

        // Toggle to incognito pane
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        mModel.get(SEARCH_LISTENER).run();
        histograms.assertExpected();
    }

    @Test
    @SmallTest
    public void testSearchBox_ClickListener_Tablet() {
        mConfiguration.screenWidthDp = WIDE_SCREEN_WIDTH_DP;
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        new HubToolbarMediator(mActivity, mModel, mPaneManager, mTracker, mSearchActivityClient);
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
                        .build();

        // Fake clicks on the search box.
        mockSearchActivityClient();
        mModel.get(SEARCH_LISTENER).run();
        verify(mSearchActivityClient).requestOmniboxForResult(any());

        // Toggle to incognito pane
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        mModel.get(SEARCH_LISTENER).run();
        histograms.assertExpected();
    }

    private void mockSearchActivityClient() {
        doReturn(mIntentBuilder).when(mSearchActivityClient).newIntentBuilder();
        doReturn(mIntentBuilder).when(mIntentBuilder).setPageUrl(any());
        doReturn(mIntentBuilder).when(mIntentBuilder).setIncognito(anyBoolean());
        doReturn(mIntentBuilder).when(mIntentBuilder).setResolutionType(anyInt());
        doReturn(mIntent).when(mIntentBuilder).build();
    }
}
