// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.hub.HubActionButtonProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubActionButtonProperties.ACTION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link HubActionButtonMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubActionButtonMediatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PaneManager mPaneManager;
    @Mock private Pane mTabSwitcherPane;
    @Mock private Pane mIncognitoTabSwitcherPane;
    @Mock private FullButtonData mFullButtonData;
    @Mock private FullButtonData mIncognitoFullButtonData;
    @Mock private HubColorMixer mColorMixer;

    private ObservableSupplierImpl<FullButtonData> mActionButtonSupplier;
    private ObservableSupplierImpl<FullButtonData> mIncognitoActionButtonSupplier;
    private ObservableSupplierImpl<Pane> mFocusedPaneSupplier;
    private PropertyModel mModel;
    private HubActionButtonMediator mMediator;

    @Before
    public void setUp() {
        mActionButtonSupplier = new ObservableSupplierImpl<>();
        mIncognitoActionButtonSupplier = new ObservableSupplierImpl<>();
        mFocusedPaneSupplier = new ObservableSupplierImpl<>();

        mModel =
                new PropertyModel.Builder(HubActionButtonProperties.ALL_ACTION_BUTTON_KEYS)
                        .with(COLOR_MIXER, mColorMixer)
                        .with(ACTION_BUTTON_VISIBLE, true)
                        .build();

        when(mPaneManager.getFocusedPaneSupplier()).thenReturn(mFocusedPaneSupplier);
        when(mTabSwitcherPane.getActionButtonDataSupplier()).thenReturn(mActionButtonSupplier);
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);

        when(mIncognitoTabSwitcherPane.getActionButtonDataSupplier())
                .thenReturn(mIncognitoActionButtonSupplier);
        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
    }

    @Test
    @SmallTest
    public void testDestroy() {
        mMediator = new HubActionButtonMediator(mModel, mPaneManager);
        assertTrue(mFocusedPaneSupplier.hasObservers());

        mMediator.destroy();
        assertFalse(mFocusedPaneSupplier.hasObservers());
    }

    @Test
    @SmallTest
    public void testWithActionButtonData() {
        mMediator = new HubActionButtonMediator(mModel, mPaneManager);
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        mActionButtonSupplier.set(mFullButtonData);
        assertEquals(mFullButtonData, mModel.get(ACTION_BUTTON_DATA));
    }

    @Test
    @SmallTest
    public void testActionButtonDataChange() {
        mMediator = new HubActionButtonMediator(mModel, mPaneManager);

        // Initially no focused pane, so action button data should be null
        assertNull(mModel.get(ACTION_BUTTON_DATA));

        // Set focused pane to tab switcher
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        assertNull(mModel.get(ACTION_BUTTON_DATA));

        // Set action button data
        mActionButtonSupplier.set(mFullButtonData);
        assertEquals(mFullButtonData, mModel.get(ACTION_BUTTON_DATA));

        // Clear action button data
        mActionButtonSupplier.set(null);
        assertNull(mModel.get(ACTION_BUTTON_DATA));
    }

    @Test
    @SmallTest
    public void testPaneSwitching() {
        mMediator = new HubActionButtonMediator(mModel, mPaneManager);

        // Set action button data for tab switcher pane
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        mActionButtonSupplier.set(mFullButtonData);
        assertEquals(mFullButtonData, mModel.get(ACTION_BUTTON_DATA));

        // Switch to incognito tab switcher pane
        mFocusedPaneSupplier.set(mIncognitoTabSwitcherPane);
        mIncognitoActionButtonSupplier.set(mIncognitoFullButtonData);
        assertEquals(mIncognitoFullButtonData, mModel.get(ACTION_BUTTON_DATA));

        // Switch back to tab switcher pane
        mFocusedPaneSupplier.set(mTabSwitcherPane);
        assertEquals(mFullButtonData, mModel.get(ACTION_BUTTON_DATA));
    }

    @Test
    @SmallTest
    public void testActionButtonVisibilityChange() {
        mMediator = new HubActionButtonMediator(mModel, mPaneManager);

        // Initially visible is true (set in setUp)
        assertTrue(mModel.get(ACTION_BUTTON_VISIBLE));

        // Test visibility change to false
        mMediator.onActionButtonVisibilityChange(false);
        assertFalse(mModel.get(ACTION_BUTTON_VISIBLE));

        // Test visibility change to true
        mMediator.onActionButtonVisibilityChange(true);
        assertTrue(mModel.get(ACTION_BUTTON_VISIBLE));

        // Test visibility change with null (should be treated as false)
        mMediator.onActionButtonVisibilityChange(null);
        assertFalse(mModel.get(ACTION_BUTTON_VISIBLE));
    }
}
