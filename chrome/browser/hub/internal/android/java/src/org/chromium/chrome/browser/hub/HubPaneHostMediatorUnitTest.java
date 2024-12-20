// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.COLOR_SCHEME;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.PANE_ROOT_VIEW;

import android.view.ViewGroup;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link HubPaneHostMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubPaneHostMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock Pane mPane;
    private @Mock Pane mIncognitoPane;
    private @Mock PaneManager mPaneManager;
    private @Mock FullButtonData mButtonData;
    private @Mock ViewGroup mRootView;

    private ObservableSupplierImpl<Pane> mPaneSupplier;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mPaneSupplier = new ObservableSupplierImpl<>();
        mModel = new PropertyModel.Builder(HubPaneHostProperties.ALL_KEYS).build();

        when(mPane.getRootView()).thenReturn(mRootView);

        when(mPaneManager.getPaneForId(PaneId.TAB_SWITCHER)).thenReturn(mPane);
        when(mPaneManager.getPaneForId(PaneId.INCOGNITO_TAB_SWITCHER)).thenReturn(mIncognitoPane);

        when(mPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        when(mPane.getColorScheme()).thenReturn(HubColorScheme.DEFAULT);
        when(mIncognitoPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        when(mIncognitoPane.getColorScheme()).thenReturn(HubColorScheme.INCOGNITO);
    }

    @Test
    @SmallTest
    public void testDestroy() {
        mPaneSupplier.set(mPane);
        HubPaneHostMediator mediator = new HubPaneHostMediator(mModel, mPaneSupplier);
        ShadowLooper.idleMainLooper();
        assertNotNull(mModel.get(PANE_ROOT_VIEW));
        assertTrue(mPaneSupplier.hasObservers());

        mediator.destroy();
        assertNull(mModel.get(PANE_ROOT_VIEW));
        assertFalse(mPaneSupplier.hasObservers());
    }

    @Test
    @SmallTest
    public void testRootView() {
        new HubPaneHostMediator(mModel, mPaneSupplier);
        assertNull(mModel.get(PANE_ROOT_VIEW));

        mPaneSupplier.set(mPane);
        assertEquals(mRootView, mModel.get(PANE_ROOT_VIEW));

        mPaneSupplier.set(null);
        assertNull(mModel.get(PANE_ROOT_VIEW));
    }

    @Test
    @SmallTest
    public void testRootView_paneAlreadySet() {
        mPaneSupplier.set(mPane);

        new HubPaneHostMediator(mModel, mPaneSupplier);
        ShadowLooper.idleMainLooper();
        assertEquals(mRootView, mModel.get(PANE_ROOT_VIEW));
    }

    @Test
    @SmallTest
    public void testHubColorScheme() {
        new HubPaneHostMediator(mModel, mPaneSupplier);
        mPaneSupplier.set(mPane);
        assertEquals(
                new HubColorSchemeUpdate(HubColorScheme.DEFAULT, HubColorScheme.DEFAULT),
                mModel.get(COLOR_SCHEME));

        mPaneSupplier.set(mIncognitoPane);
        assertEquals(
                new HubColorSchemeUpdate(HubColorScheme.INCOGNITO, HubColorScheme.DEFAULT),
                mModel.get(COLOR_SCHEME));

        mPaneSupplier.set(null);
        assertEquals(
                new HubColorSchemeUpdate(HubColorScheme.DEFAULT, HubColorScheme.INCOGNITO),
                mModel.get(COLOR_SCHEME));
    }
}
