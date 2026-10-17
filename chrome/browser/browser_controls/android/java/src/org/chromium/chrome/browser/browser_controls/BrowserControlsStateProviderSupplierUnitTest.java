// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link BrowserControlsStateProviderSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BrowserControlsStateProviderSupplierUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BrowserControlsStateProvider mProvider;
    @Mock private Callback<@Nullable BrowserControlsStateProvider> mObserver;

    private WindowAndroid mWindowAndroid;
    private final SettableMonotonicObservableSupplier<BrowserControlsStateProvider> mSupplier =
            ObservableSuppliers.createMonotonic();

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        mWindowAndroid = new WindowAndroid(activity, /* occlusionTrackingAllowed= */ false);
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
    }

    @Test
    public void testNullWindow() {
        assertNull(BrowserControlsStateProviderSupplier.from(null));
        assertNull(BrowserControlsStateProviderSupplier.getValueOrNullFrom(null));
    }

    @Test
    public void testNothingAttached() {
        assertNull(BrowserControlsStateProviderSupplier.from(mWindowAndroid));
        assertNull(BrowserControlsStateProviderSupplier.getValueOrNullFrom(mWindowAndroid));
    }

    @Test
    public void testAttach() {
        BrowserControlsStateProviderSupplier.attach(
                mWindowAndroid.getUnownedUserDataHost(), mSupplier);
        // Attached, but the provider has not been created yet.
        assertNull(BrowserControlsStateProviderSupplier.getValueOrNullFrom(mWindowAndroid));
        BrowserControlsStateProviderSupplier.from(mWindowAndroid).addSyncObserver(mObserver);

        mSupplier.set(mProvider);

        verify(mObserver).onResult(mProvider);
        assertEquals(
                mProvider, BrowserControlsStateProviderSupplier.getValueOrNullFrom(mWindowAndroid));
    }

    @Test
    public void testDestroy() {
        mSupplier.set(mProvider);
        BrowserControlsStateProviderSupplier.attach(
                mWindowAndroid.getUnownedUserDataHost(), mSupplier);

        BrowserControlsStateProviderSupplier.destroy(mSupplier);

        assertNull(BrowserControlsStateProviderSupplier.from(mWindowAndroid));
    }
}
