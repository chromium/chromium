// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.hub.HubBottomToolbarProperties.BOTTOM_TOOLBAR_VISIBLE;
import static org.chromium.chrome.browser.hub.HubBottomToolbarProperties.COLOR_SCHEME;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link HubBottomToolbarMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubBottomToolbarMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mRegularTab;
    @Mock private Tab mIncognitoTab;

    private PropertyModel mModel;
    private final SettableNonNullObservableSupplier<Boolean> mVisibilitySupplier =
            ObservableSuppliers.createNonNull(false);
    private HubBottomToolbarDelegate mDelegate;

    @Before
    public void setUp() {
        when(mRegularTab.isIncognito()).thenReturn(false);
        when(mIncognitoTab.isIncognito()).thenReturn(true);

        mModel = new PropertyModel.Builder(HubBottomToolbarProperties.ALL_BOTTOM_KEYS).build();
        mDelegate =
                new EmptyHubBottomToolbarDelegate() {
                    @Override
                    public NonNullObservableSupplier<Boolean> getBottomToolbarVisibilitySupplier() {
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
        assertEquals(false, mModel.get(BOTTOM_TOOLBAR_VISIBLE));

        // Change to visible
        mVisibilitySupplier.set(true);
        assertEquals(true, mModel.get(BOTTOM_TOOLBAR_VISIBLE));

        // Change back to hidden
        mVisibilitySupplier.set(false);
        assertEquals(false, mModel.get(BOTTOM_TOOLBAR_VISIBLE));

        mediator.destroy();
    }

    @Test
    @SmallTest
    public void testCurrentTabSupplier_WhenNotHiding_DoesNotUpdateColorScheme() {
        SettableNullableObservableSupplier<Tab> currentTabSupplier =
                ObservableSuppliers.createNullable();
        SettableNonNullObservableSupplier<Boolean> isHidingSupplier =
                ObservableSuppliers.createNonNull(false);

        mModel.set(COLOR_SCHEME, HubColorScheme.INCOGNITO);
        currentTabSupplier.set(mIncognitoTab);

        HubBottomToolbarMediator mediator =
                new HubBottomToolbarMediator(
                        mModel, mDelegate, currentTabSupplier, isHidingSupplier);

        // When in Incognito GTS and not hiding, closing the last incognito tab switches
        // current tab to regular, but should NOT update the HubBottomToolbar COLOR_SCHEME.
        currentTabSupplier.set(mRegularTab);
        assertEquals(HubColorScheme.INCOGNITO, mModel.get(COLOR_SCHEME));

        mediator.destroy();
        assertFalse(currentTabSupplier.hasObservers());
        assertFalse(isHidingSupplier.hasObservers());
    }

    @Test
    @SmallTest
    public void testCurrentTabSupplier_WhenHiding_UpdatesColorScheme() {
        SettableNullableObservableSupplier<Tab> currentTabSupplier =
                ObservableSuppliers.createNullable();
        SettableNonNullObservableSupplier<Boolean> isHidingSupplier =
                ObservableSuppliers.createNonNull(true);

        currentTabSupplier.set(mRegularTab);

        HubBottomToolbarMediator mediator =
                new HubBottomToolbarMediator(
                        mModel, mDelegate, currentTabSupplier, isHidingSupplier);

        assertEquals(HubColorScheme.DEFAULT, mModel.get(COLOR_SCHEME));

        // Switch to incognito tab while hiding
        currentTabSupplier.set(mIncognitoTab);
        assertEquals(HubColorScheme.INCOGNITO, mModel.get(COLOR_SCHEME));

        // Switch back to regular tab while hiding
        currentTabSupplier.set(mRegularTab);
        assertEquals(HubColorScheme.DEFAULT, mModel.get(COLOR_SCHEME));

        mediator.destroy();
        assertFalse(currentTabSupplier.hasObservers());
        assertFalse(isHidingSupplier.hasObservers());
    }

    @Test
    @SmallTest
    public void testCurrentTabSupplier_WhenHidingWithNullTab_DefaultsToDefaultColorScheme() {
        SettableNullableObservableSupplier<Tab> currentTabSupplier =
                ObservableSuppliers.createNullable();
        SettableNonNullObservableSupplier<Boolean> isHidingSupplier =
                ObservableSuppliers.createNonNull(true);

        currentTabSupplier.set(mIncognitoTab);

        HubBottomToolbarMediator mediator =
                new HubBottomToolbarMediator(
                        mModel, mDelegate, currentTabSupplier, isHidingSupplier);

        assertEquals(HubColorScheme.INCOGNITO, mModel.get(COLOR_SCHEME));

        // When tab is set to null while hiding, color scheme defaults to DEFAULT
        currentTabSupplier.set(null);
        assertEquals(HubColorScheme.DEFAULT, mModel.get(COLOR_SCHEME));

        mediator.destroy();
        assertFalse(currentTabSupplier.hasObservers());
        assertFalse(isHidingSupplier.hasObservers());
    }

    @Test
    @SmallTest
    public void testIsHidingStateChange_UpdatesColorScheme() {
        SettableNullableObservableSupplier<Tab> currentTabSupplier =
                ObservableSuppliers.createNullable();
        SettableNonNullObservableSupplier<Boolean> isHidingSupplier =
                ObservableSuppliers.createNonNull(false);

        mModel.set(COLOR_SCHEME, HubColorScheme.INCOGNITO);
        currentTabSupplier.set(mRegularTab);

        HubBottomToolbarMediator mediator =
                new HubBottomToolbarMediator(
                        mModel, mDelegate, currentTabSupplier, isHidingSupplier);

        // While not hiding, COLOR_SCHEME is untouched
        assertEquals(HubColorScheme.INCOGNITO, mModel.get(COLOR_SCHEME));

        // When Hub starts hiding, destination tab color scheme is immediately applied
        isHidingSupplier.set(true);
        assertEquals(HubColorScheme.DEFAULT, mModel.get(COLOR_SCHEME));

        // When current tab changes while hiding
        currentTabSupplier.set(mIncognitoTab);
        assertEquals(HubColorScheme.INCOGNITO, mModel.get(COLOR_SCHEME));

        // When Hub finishes hiding / resets isHiding to false, onHidingChanged does nothing
        isHidingSupplier.set(false);
        assertEquals(HubColorScheme.INCOGNITO, mModel.get(COLOR_SCHEME));

        // When isHiding is false, setting currentTab does not update COLOR_SCHEME
        currentTabSupplier.set(mRegularTab);
        assertEquals(HubColorScheme.INCOGNITO, mModel.get(COLOR_SCHEME));

        mediator.destroy();
        assertFalse(currentTabSupplier.hasObservers());
        assertFalse(isHidingSupplier.hasObservers());
    }
}
