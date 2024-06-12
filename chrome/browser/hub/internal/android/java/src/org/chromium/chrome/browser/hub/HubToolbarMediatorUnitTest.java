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
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.hub.HubToolbarProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.COLOR_SCHEME;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.MENU_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_BUTTON_LOOKUP_CALLBACK;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_INDEX;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SHOW_ACTION_BUTTON_TEXT;

import android.view.View;

import androidx.test.filters.SmallTest;

import com.google.common.collect.ImmutableSet;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.cached_flags.CachedFlagUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.HubToolbarProperties.PaneButtonLookup;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

import java.util.Arrays;
import java.util.List;

/** Tests for {@link HubToolbarMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON)
public class HubToolbarMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PaneManager mPaneManager;
    @Mock private Pane mTabSwitcherPane;
    @Mock private Pane mIncognitoTabSwitcherPane;
    @Mock private FullButtonData mFullButtonData;
    @Mock private PaneOrderController mPaneOrderController;
    @Mock private DisplayButtonData mDisplayButtonData;
    @Mock private PropertyObserver<PropertyKey> mPropertyObserver;
    @Mock private View mButton1;
    @Mock private View mButton2;
    @Mock private Tracker mTracker;

    private ObservableSupplierImpl<FullButtonData> mActionButtonSupplier;
    private ObservableSupplierImpl<Pane> mFocusedPaneSupplier;
    private ObservableSupplierImpl<DisplayButtonData> mTabSwitcherReferenceButtonDataSupplier1;
    private ObservableSupplierImpl<DisplayButtonData>
            mIncognitoTabSwitcherReferenceButtonDataSupplier2;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActionButtonSupplier = new ObservableSupplierImpl<>();
        mFocusedPaneSupplier = new ObservableSupplierImpl<>();
        mTabSwitcherReferenceButtonDataSupplier1 = new ObservableSupplierImpl<>();
        mIncognitoTabSwitcherReferenceButtonDataSupplier2 = new ObservableSupplierImpl<>();
        mModel = new PropertyModel.Builder(HubToolbarProperties.ALL_KEYS).build();
        mModel.addObserver(mPropertyObserver);

        when(mPaneOrderController.getPaneOrder())
                .thenReturn(
                        ImmutableSet.of(
                                PaneId.TAB_SWITCHER,
                                PaneId.INCOGNITO_TAB_SWITCHER,
                                PaneId.BOOKMARKS));
        when(mPaneManager.getFocusedPaneSupplier()).thenReturn(mFocusedPaneSupplier);
        when(mPaneManager.getPaneOrderController()).thenReturn(mPaneOrderController);
        when(mPaneManager.getPaneForId(PaneId.TAB_SWITCHER)).thenReturn(mTabSwitcherPane);
        when(mPaneManager.getPaneForId(PaneId.INCOGNITO_TAB_SWITCHER))
                .thenReturn(mIncognitoTabSwitcherPane);

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
    }

    @After
    public void tearDown() {
        CachedFlagUtils.resetFlagsForTesting();
    }

    @Test
    @SmallTest
    public void testDestroy() {
        assertFalse(mFocusedPaneSupplier.hasObservers());

        HubToolbarMediator mediator = new HubToolbarMediator(mModel, mPaneManager, mTracker);
        assertTrue(mFocusedPaneSupplier.hasObservers());

        mediator.destroy();
        assertFalse(mFocusedPaneSupplier.hasObservers());

        assertFalse(mTabSwitcherReferenceButtonDataSupplier1.hasObservers());
        assertFalse(mIncognitoTabSwitcherReferenceButtonDataSupplier2.hasObservers());
    }

    @Test
    @SmallTest
    public void testWithActionButtonData() {
        new HubToolbarMediator(mModel, mPaneManager, mTracker);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        mActionButtonSupplier.set(mFullButtonData);
        assertEquals(mFullButtonData, mModel.get(ACTION_BUTTON_DATA));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON)
    public void testNoActionButtonData() {
        new HubToolbarMediator(mModel, mPaneManager, mTracker);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        mActionButtonSupplier.set(mFullButtonData);
        assertEquals(null, mModel.get(ACTION_BUTTON_DATA));
    }

    @Test
    @SmallTest
    public void testPaneSwitcherButtonData() {
        new HubToolbarMediator(mModel, mPaneManager, mTracker);
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

        new HubToolbarMediator(mModel, mPaneManager, mTracker);
        List<FullButtonData> paneSwitcherButtonData = mModel.get(PANE_SWITCHER_BUTTON_DATA);
        assertEquals(1, paneSwitcherButtonData.size());

        paneSwitcherButtonData.get(0).getOnPressRunnable().run();
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
    }

    @Test
    @SmallTest
    public void testPaneSwitcherButtonDataEventCount() {
        verify(mPropertyObserver, never()).onPropertyChanged(any(), eq(PANE_SWITCHER_BUTTON_DATA));

        new HubToolbarMediator(mModel, mPaneManager, mTracker);
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
    public void testActionButtonHasText() {
        new HubToolbarMediator(mModel, mPaneManager, mTracker);
        assertFalse(mModel.get(SHOW_ACTION_BUTTON_TEXT));

        mTabSwitcherReferenceButtonDataSupplier1.set(null);
        assertTrue(mModel.get(SHOW_ACTION_BUTTON_TEXT));

        mIncognitoTabSwitcherReferenceButtonDataSupplier2.set(null);
        assertTrue(mModel.get(SHOW_ACTION_BUTTON_TEXT));
    }

    @Test
    @SmallTest
    public void testPaneSwitcherIndex() {
        new HubToolbarMediator(mModel, mPaneManager, mTracker);
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
    public void testHubColorScheme() {
        new HubToolbarMediator(mModel, mPaneManager, mTracker);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        assertEquals(HubColorScheme.DEFAULT, mModel.get(COLOR_SCHEME));

        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertEquals(HubColorScheme.INCOGNITO, mModel.get(COLOR_SCHEME));

        mFocusedPaneSupplier.set(null);
        assertEquals(HubColorScheme.DEFAULT, mModel.get(COLOR_SCHEME));
    }

    @Test
    @SmallTest
    public void testMenuButtonVisibility() {
        new HubToolbarMediator(mModel, mPaneManager, mTracker);
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
        HubToolbarMediator mediator = new HubToolbarMediator(mModel, mPaneManager, mTracker);
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

        new HubToolbarMediator(mModel, mPaneManager, mTracker);
        List<FullButtonData> paneSwitcherButtonData = mModel.get(PANE_SWITCHER_BUTTON_DATA);
        assertEquals(2, paneSwitcherButtonData.size());

        paneSwitcherButtonData.get(0).getOnPressRunnable().run();
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
        verifyNoInteractions(mTracker);

        paneSwitcherButtonData.get(1).getOnPressRunnable().run();
        verify(mPaneManager).focusPane(PaneId.TAB_GROUPS);
        verify(mTracker).notifyEvent("tab_groups_surface_clicked");
    }
}
