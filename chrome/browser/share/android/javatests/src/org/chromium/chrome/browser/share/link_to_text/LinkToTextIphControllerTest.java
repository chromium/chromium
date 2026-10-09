// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Unit tests for {@link LinkToTextIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LinkToTextIphControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Profile mProfile;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private final SettableNullableObservableSupplier<Tab> mTabSupplier =
            ObservableSuppliers.createNullable();
    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();

    @Before
    public void setUp() {
        mTabSupplier.set(mTab);
        mProfileSupplier.set(mProfile);
    }

    @Test
    public void testDestroyRemovesTabObservers() {
        LinkToTextIphController controller =
                new LinkToTextIphController(mTabSupplier, mTabModelSelector, mProfileSupplier);
        ShadowLooper.idleMainLooper();
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        assertTrue(mTabSupplier.hasObservers());

        controller.destroy();

        verify(mTab).removeObserver(mTabObserverCaptor.getValue());
        assertFalse(mTabSupplier.hasObservers());
    }
}
