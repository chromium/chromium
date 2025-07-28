// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TopInsetCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class TopInsetCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int TOP_PADDING = 50;
    @Mock private InsetObserver mInsetObserver;
    @Mock private Tab mNtpTab;
    @Mock private Tab mNonNtpTab1;
    @Mock private Tab mNonNtpTab2;
    @Mock private WindowInsetsCompat mWindowInsetsCompat;
    @Mock private ObservableSupplierImpl<@Nullable Tab> mTabObservableSupplier;
    @Mock private View mView;
    @Mock private NativePage mNativePage;
    @Mock private TopInsetCoordinator.Observer mObserver;

    private TopInsetCoordinator mTopInsetCoordinator;

    @Before
    public void setUp() {
        when(mNtpTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mNtpTab.isNativePage()).thenReturn(true);
        when(mNtpTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.supportsEdgeToEdgeOnTop()).thenReturn(true);

        when(mNonNtpTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mNonNtpTab1.isNativePage()).thenReturn(false);
        when(mNonNtpTab1.getNativePage()).thenReturn(null);

        when(mNonNtpTab2.getUrl()).thenReturn(JUnitTestGURLs.URL_2);
        when(mNonNtpTab2.isNativePage()).thenReturn(false);
        when(mNonNtpTab2.getNativePage()).thenReturn(null);

        mTopInsetCoordinator = new TopInsetCoordinator(mTabObservableSupplier, mInsetObserver);
        mTopInsetCoordinator.addObserver(mObserver);

        mWindowInsetsCompat = createWindowInsetsCompat(TOP_PADDING);
    }

    @Test
    public void testOnApplyWindowInsets_ConsumeTopInset() {
        Mockito.clearInvocations(mObserver);
        mTopInsetCoordinator.onTabSwitched(mNtpTab);

        assertNotNull(mWindowInsetsCompat.getInsets(WindowInsetsCompat.Type.systemBars()));
        assertNotNull(mWindowInsetsCompat.getInsets(WindowInsetsCompat.Type.displayCutout()));
        WindowInsetsCompat result =
                mTopInsetCoordinator.onApplyWindowInsets(mView, mWindowInsetsCompat);

        // Verify that the top inset is consumed for NTP.
        assertNotEquals(mWindowInsetsCompat, result);
        assertEquals(Insets.NONE, result.getInsets(WindowInsetsCompat.Type.systemBars()));
        assertEquals(Insets.NONE, result.getInsets(WindowInsetsCompat.Type.displayCutout()));
        verify(mObserver).onToEdgeChange(eq(TOP_PADDING), eq(true));
    }

    @Test
    public void testOnApplyWindowInsets_DoNotConsumeTopInset() {
        when(mNativePage.supportsEdgeToEdgeOnTop()).thenReturn(false);
        Mockito.clearInvocations(mObserver);
        mTopInsetCoordinator.onTabSwitched(mNonNtpTab1);

        WindowInsetsCompat result =
                mTopInsetCoordinator.onApplyWindowInsets(mView, mWindowInsetsCompat);

        // Verify that the top inset is not consumed for non-NTP tab.
        assertEquals(mWindowInsetsCompat, result);
        verify(mObserver).onToEdgeChange(eq(TOP_PADDING), eq(false));
    }

    @Test
    public void testOnTabSwitched_RetriggerOnApplyWindowInsets() {
        // Verifies that retriggerOnApplyWindowInsets() is called if the new tab is a NTP.
        Mockito.clearInvocations(mInsetObserver);
        mTopInsetCoordinator.onTabSwitched(mNtpTab);
        verify(mInsetObserver).retriggerOnApplyWindowInsets();

        // Updates |mConsumeTopInset| for the current NTP.
        mTopInsetCoordinator.onApplyWindowInsets(mView, mWindowInsetsCompat);
        // Verifies |mConsumeTopInset| is updated by setting to true, i.e, the current Tab is a NTP.
        assertTrue(mTopInsetCoordinator.getConsumeTopInsetForTesting());

        // Verifies that retriggerOnApplyWindowInsets() is called if the previous tab is a NTP.
        mTopInsetCoordinator.onTabSwitched(mNonNtpTab1);
        verify(mInsetObserver, times(2)).retriggerOnApplyWindowInsets();
        // Updates |mConsumeTopInset| for the current non NTP Tab.
        mTopInsetCoordinator.onApplyWindowInsets(mView, mWindowInsetsCompat);
        // Verifies |mConsumeTopInset| is updated by setting to false, i.e., the current Tab isn't a
        // NTP.
        assertFalse(mTopInsetCoordinator.getConsumeTopInsetForTesting());

        // Verifies that retriggerOnApplyWindowInsets() is NOT called if none of the new tab or the
        // previous tab is a NTP.
        Mockito.clearInvocations(mInsetObserver);
        mTopInsetCoordinator.onTabSwitched(mNonNtpTab2);
        verify(mInsetObserver, never()).retriggerOnApplyWindowInsets();
    }

    @Test
    public void testOnTabSwitched_NullTab() {
        Mockito.clearInvocations(mInsetObserver);
        mTopInsetCoordinator.onTabSwitched(null);
        verify(mInsetObserver, never()).retriggerOnApplyWindowInsets();
    }

    @Test
    public void testDestroy() {
        mTopInsetCoordinator.onTabSwitched(mNonNtpTab1);
        mTopInsetCoordinator.destroy();

        mInsetObserver.removeInsetsConsumer(eq(mTopInsetCoordinator));
        verify(mNonNtpTab1).removeObserver(any(TabObserver.class));
        assertEquals(0, mTopInsetCoordinator.getObserverCountForTesting());
    }

    private WindowInsetsCompat createWindowInsetsCompat(int top) {
        Insets systemInsets = Insets.of(0, top, 0, 0);
        Insets displayCutoutInsets = Insets.of(0, top, 0, 0);
        var builder = new WindowInsetsCompat.Builder();
        return builder.setInsets(WindowInsetsCompat.Type.systemBars(), systemInsets)
                .setInsets(WindowInsetsCompat.Type.displayCutout(), displayCutoutInsets)
                .build();
    }
}
