// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;

/** Tests for the TabSupplierObserver. */
@Batch(Batch.PER_CLASS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabSupplierObserverTest {
    @Mock private Tab mMockTab;

    @Mock private Tab mAnotherMockedTab;

    private ObservableSupplierImpl<Tab> mObservableTabSupplier =
            new ObservableSupplierImpl<>(mMockTab);

    /** A test observer that provides access to the tab being observed. */
    private static class TestTabSupplierObserver extends TabSupplierObserver {
        /** The tab currently being observed. */
        private Tab mObservedTab;

        private boolean mDidCallObservingDifferentTab;

        public TestTabSupplierObserver(ObservableSupplier<Tab> provider) {
            this(provider, false);
        }

        public TestTabSupplierObserver(ObservableSupplier<Tab> provider, boolean shouldTrigger) {
            super(provider, shouldTrigger);
            mObservedTab = provider.get();
        }

        @Override
        public void onObservingDifferentTab(Tab tab) {
            mDidCallObservingDifferentTab = true;
            mObservedTab = tab;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
    }

    private Tab getCurrentTab() {
        return mObservableTabSupplier.get();
    }

    /** Test that the {@link TabSupplierObserver} switches between tabs as the tab changes. */
    @Test
    @SmallTest
    @Feature({"TabSupplierObserver"})
    public void basicChangeTracking() {
        Tab startingTab = getCurrentTab();

        TestTabSupplierObserver tabSupplierObserver =
                new TestTabSupplierObserver(mObservableTabSupplier);
        assertFalse(
                "Unexpected initial call to onObservingDifferentTab!",
                tabSupplierObserver.mDidCallObservingDifferentTab);
        assertEquals(
                "The observer should be attached to the starting tab.",
                startingTab,
                tabSupplierObserver.mObservedTab);

        // Now tell the supplier to switch tabs.
        mObservableTabSupplier.set(mAnotherMockedTab);

        assertNotEquals("The tab should have changed.", startingTab, getCurrentTab());
        assertEquals(
                "The observer should be attached to the new tab.",
                getCurrentTab(),
                tabSupplierObserver.mObservedTab);

        tabSupplierObserver.destroy();
    }

    @Test
    @SmallTest
    @Feature({"TabSupplierObserver"})
    public void initialTrigger() {
        TestTabSupplierObserver tabSupplierObserver =
                new TestTabSupplierObserver(mObservableTabSupplier, true);
        assertTrue(
                "Expected initial call to onObservingDifferentTab!",
                tabSupplierObserver.mDidCallObservingDifferentTab);
    }

    @Test
    @SmallTest
    @Feature({"TabSupplierObserver"})
    public void setToNull() {
        mObservableTabSupplier.set(null);
        assertNull("Expected that the tab is now null", getCurrentTab());
    }
}
