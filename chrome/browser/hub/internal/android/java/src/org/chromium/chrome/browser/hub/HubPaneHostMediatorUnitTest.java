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

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.PANE_ROOT_VIEW;

import android.view.ViewGroup;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.cached_flags.CachedFlagUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link HubPaneHostMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON)
public class HubPaneHostMediatorUnitTest {
    @Rule public TestRule mFeatureProcessor = new Features.JUnitProcessor();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock Pane mPane;
    private @Mock FullButtonData mButtonData;
    private @Mock ViewGroup mRootView;

    private ObservableSupplierImpl<Pane> mPaneSupplier;
    private ObservableSupplierImpl<FullButtonData> mActionButtonSupplier;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mPaneSupplier = new ObservableSupplierImpl<>();
        mActionButtonSupplier = new ObservableSupplierImpl<>();
        mModel = new PropertyModel.Builder(HubPaneHostProperties.ALL_KEYS).build();

        when(mPane.getRootView()).thenReturn(mRootView);
        when(mPane.getActionButtonDataSupplier()).thenReturn(mActionButtonSupplier);
    }

    @After
    public void tearDown() {
        CachedFlagUtils.resetFlagsForTesting();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON)
    public void testDestroy() {
        mPaneSupplier.set(mPane);
        HubPaneHostMediator mediator = new HubPaneHostMediator(mModel, mPaneSupplier);
        ShadowLooper.idleMainLooper();
        assertNotNull(mModel.get(PANE_ROOT_VIEW));
        assertTrue(mPaneSupplier.hasObservers());
        assertTrue(mActionButtonSupplier.hasObservers());

        mediator.destroy();
        assertNull(mModel.get(PANE_ROOT_VIEW));
        assertFalse(mPaneSupplier.hasObservers());
        assertFalse(mActionButtonSupplier.hasObservers());
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
    @EnableFeatures(ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON)
    public void testActionButtonData() {
        new HubPaneHostMediator(mModel, mPaneSupplier);
        assertNull(mModel.get(ACTION_BUTTON_DATA));

        mPaneSupplier.set(mPane);
        assertNull(mModel.get(ACTION_BUTTON_DATA));

        mActionButtonSupplier.set(mButtonData);
        assertEquals(mButtonData, mModel.get(ACTION_BUTTON_DATA));

        mActionButtonSupplier.set(null);
        assertEquals(null, mModel.get(ACTION_BUTTON_DATA));

        mActionButtonSupplier.set(mButtonData);
        assertEquals(mButtonData, mModel.get(ACTION_BUTTON_DATA));

        mPaneSupplier.set(null);
        assertEquals(null, mModel.get(ACTION_BUTTON_DATA));

        mPaneSupplier.set(mPane);
        // Supplier impl is going to post the already set value on addObserver, so idle is needed.
        ShadowLooper.idleMainLooper();
        assertEquals(mButtonData, mModel.get(ACTION_BUTTON_DATA));
    }

    @Test
    @SmallTest
    public void testDisabledActionButtonData() {
        new HubPaneHostMediator(mModel, mPaneSupplier);
        mPaneSupplier.set(mPane);
        mActionButtonSupplier.set(mButtonData);
        assertNull(mModel.get(ACTION_BUTTON_DATA));
    }
}
