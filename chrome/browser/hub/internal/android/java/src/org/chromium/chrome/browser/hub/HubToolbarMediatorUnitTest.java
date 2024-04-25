// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.hub.HubToolbarProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.COLOR_SCHEME;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.MENU_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_INDEX;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SHOW_ACTION_BUTTON_TEXT;

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

import org.chromium.base.cached_flags.CachedFlagUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

import java.util.List;

/** Tests for {@link HubToolbarMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubToolbarMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PaneManager mPaneManager;
    @Mock private Pane mTabSwitcherPane;
    @Mock private Pane mIncognitoTabSwitcherPane;
    @Mock private FullButtonData mFullButtonData;
    @Mock private PaneOrderController mPaneOrderController;
    @Mock private DisplayButtonData mDisplayButtonData;
    @Mock private PropertyObserver<PropertyKey> mPropertyObserver;

    private ObservableSupplierImpl<FullButtonData> mActionButtonSupplier;
    private ObservableSupplierImpl<Pane> mFocusedPaneSupplier;
    private ObservableSupplierImpl<DisplayButtonData> mTabSwitcherReferenceButtonDataSupplier1;
    private ObservableSupplierImpl<DisplayButtonData> mBookmarksReferenceButtonDataSupplier2;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActionButtonSupplier = new ObservableSupplierImpl<>();
        mFocusedPaneSupplier = new ObservableSupplierImpl<>();
        mTabSwitcherReferenceButtonDataSupplier1 = new ObservableSupplierImpl<>();
        mBookmarksReferenceButtonDataSupplier2 = new ObservableSupplierImpl<>();
        mModel = new PropertyModel.Builder(HubToolbarProperties.ALL_KEYS).build();
        mModel.addObserver(mPropertyObserver);

        when(mPaneOrderController.getPaneOrder())
                .thenReturn(ImmutableSet.of(PaneId.TAB_SWITCHER, PaneId.INCOGNITO_TAB_SWITCHER));
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
                .thenReturn(mBookmarksReferenceButtonDataSupplier2);
        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        when(mIncognitoTabSwitcherPane.getColorScheme()).thenReturn(HubColorScheme.INCOGNITO);

        mTabSwitcherReferenceButtonDataSupplier1.set(mDisplayButtonData);
        mBookmarksReferenceButtonDataSupplier2.set(mDisplayButtonData);
    }

    @After
    public void tearDown() {
        CachedFlagUtils.resetFlagsForTesting();
    }

    @Test
    @SmallTest
    public void testDestroy() {
        assertFalse(mFocusedPaneSupplier.hasObservers());

        HubToolbarMediator mediator = new HubToolbarMediator(mModel, mPaneManager);
        assertTrue(mFocusedPaneSupplier.hasObservers());

        mediator.destroy();
        assertFalse(mFocusedPaneSupplier.hasObservers());

        assertFalse(mTabSwitcherReferenceButtonDataSupplier1.hasObservers());
        assertFalse(mBookmarksReferenceButtonDataSupplier2.hasObservers());
    }

    @Test
    @SmallTest
    public void testWithActionButtonData() {
        HubFieldTrial.FLOATING_ACTION_BUTTON.setForTesting(false);
        new HubToolbarMediator(mModel, mPaneManager);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        mActionButtonSupplier.set(mFullButtonData);
        assertEquals(mFullButtonData, mModel.get(ACTION_BUTTON_DATA));
    }

    @Test
    @SmallTest
    public void testNoActionButtonData() {
        HubFieldTrial.FLOATING_ACTION_BUTTON.setForTesting(true);
        new HubToolbarMediator(mModel, mPaneManager);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        mActionButtonSupplier.set(mFullButtonData);
        assertEquals(null, mModel.get(ACTION_BUTTON_DATA));
    }

    @Test
    @SmallTest
    public void testPaneSwitcherButtonData() {
        new HubToolbarMediator(mModel, mPaneManager);
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

        new HubToolbarMediator(mModel, mPaneManager);
        List<FullButtonData> paneSwitcherButtonData = mModel.get(PANE_SWITCHER_BUTTON_DATA);
        assertEquals(1, paneSwitcherButtonData.size());

        paneSwitcherButtonData.get(0).getOnPressRunnable().run();
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
    }

    @Test
    @SmallTest
    public void testPaneSwitcherButtonDataEventCount() {
        verify(mPropertyObserver, never()).onPropertyChanged(any(), eq(PANE_SWITCHER_BUTTON_DATA));

        new HubToolbarMediator(mModel, mPaneManager);
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
        new HubToolbarMediator(mModel, mPaneManager);
        assertFalse(mModel.get(SHOW_ACTION_BUTTON_TEXT));

        mTabSwitcherReferenceButtonDataSupplier1.set(null);
        assertTrue(mModel.get(SHOW_ACTION_BUTTON_TEXT));

        mBookmarksReferenceButtonDataSupplier2.set(null);
        assertTrue(mModel.get(SHOW_ACTION_BUTTON_TEXT));
    }

    @Test
    @SmallTest
    public void testPaneSwitcherIndex() {
        new HubToolbarMediator(mModel, mPaneManager);
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
        new HubToolbarMediator(mModel, mPaneManager);
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
        new HubToolbarMediator(mModel, mPaneManager);
        when(mIncognitoTabSwitcherPane.getMenuButtonVisible()).thenReturn(false);
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertFalse(mModel.get(MENU_BUTTON_VISIBLE));

        when(mTabSwitcherPane.getMenuButtonVisible()).thenReturn(true);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        assertTrue(mModel.get(MENU_BUTTON_VISIBLE));

        mFocusedPaneSupplier.set(null);
        assertFalse(mModel.get(MENU_BUTTON_VISIBLE));
    }
}
