// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link HubPaneHostMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubPaneHostMediatorUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock Pane mPane;
    private @Mock FullButtonData mButtonData;

    private final ObservableSupplierImpl<Pane> mPaneSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<FullButtonData> mActionButtonSupplier =
            new ObservableSupplierImpl<>();
    private final PropertyModel mModel =
            new PropertyModel.Builder(HubPaneHostProperties.ALL_KEYS).build();

    @Before
    public void setUp() {
        Mockito.when(mPane.getActionButtonDataSupplier()).thenReturn(mActionButtonSupplier);
    }

    @Test
    @SmallTest
    public void testActionButtonData() {
        HubFieldTrial.FLOATING_ACTION_BUTTON.setForTesting(true);
        new HubPaneHostMediator(mModel, mPaneSupplier);
        assertNull(mModel.get(HubPaneHostProperties.ACTION_BUTTON_DATA));

        mPaneSupplier.set(mPane);
        assertNull(mModel.get(HubPaneHostProperties.ACTION_BUTTON_DATA));

        mActionButtonSupplier.set(mButtonData);
        assertEquals(mButtonData, mModel.get(HubPaneHostProperties.ACTION_BUTTON_DATA));

        mActionButtonSupplier.set(null);
        assertEquals(null, mModel.get(HubPaneHostProperties.ACTION_BUTTON_DATA));

        mActionButtonSupplier.set(mButtonData);
        assertEquals(mButtonData, mModel.get(HubPaneHostProperties.ACTION_BUTTON_DATA));

        mPaneSupplier.set(null);
        assertEquals(null, mModel.get(HubPaneHostProperties.ACTION_BUTTON_DATA));

        mPaneSupplier.set(mPane);
        // Supplier impl is going to post the already set value on addObserver, so idle is needed.
        ShadowLooper.idleMainLooper();
        assertEquals(mButtonData, mModel.get(HubPaneHostProperties.ACTION_BUTTON_DATA));
    }

    @Test
    @SmallTest
    public void testDisabledActionButtonData() {
        HubFieldTrial.FLOATING_ACTION_BUTTON.setForTesting(false);
        new HubPaneHostMediator(mModel, mPaneSupplier);
        mPaneSupplier.set(mPane);
        mActionButtonSupplier.set(mButtonData);
        assertNull(mModel.get(HubPaneHostProperties.ACTION_BUTTON_DATA));
    }
}
