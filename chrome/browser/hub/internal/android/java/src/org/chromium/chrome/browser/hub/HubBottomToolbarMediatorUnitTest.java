// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.hub.HubBottomToolbarProperties.BOTTOM_TOOLBAR_VISIBLE;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link HubBottomToolbarMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubBottomToolbarMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private PropertyModel mModel;
    private ObservableSupplierImpl<Boolean> mVisibilitySupplier;
    private HubBottomToolbarDelegate mDelegate;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(HubBottomToolbarProperties.ALL_BOTTOM_KEYS).build();

        mVisibilitySupplier = new ObservableSupplierImpl<>();
        mDelegate =
                new EmptyHubBottomToolbarDelegate() {
                    @Override
                    public ObservableSupplierImpl<Boolean> getBottomToolbarVisibilitySupplier() {
                        return mVisibilitySupplier;
                    }
                };
    }

    @Test
    @SmallTest
    public void testMediatorWithDelegate() {
        HubBottomToolbarMediator mediator = new HubBottomToolbarMediator(mModel, mDelegate);

        // Verify that the mediator observes the visibility supplier
        assertTrue(mVisibilitySupplier.hasObservers());

        mediator.destroy();

        // After destroy, observers should be removed
        assertFalse(mVisibilitySupplier.hasObservers());
    }

    @Test
    @SmallTest
    public void testVisibilityChanges() {
        HubBottomToolbarMediator mediator = new HubBottomToolbarMediator(mModel, mDelegate);

        // Initially should be false (default from empty supplier)
        mVisibilitySupplier.set(false);
        assertEquals(false, mModel.get(BOTTOM_TOOLBAR_VISIBLE));

        // Change to visible
        mVisibilitySupplier.set(true);
        assertEquals(true, mModel.get(BOTTOM_TOOLBAR_VISIBLE));

        // Change back to hidden
        mVisibilitySupplier.set(false);
        assertEquals(false, mModel.get(BOTTOM_TOOLBAR_VISIBLE));

        // Test null value handling
        mVisibilitySupplier.set(null);
        assertEquals(false, mModel.get(BOTTOM_TOOLBAR_VISIBLE));

        mediator.destroy();
    }
}
