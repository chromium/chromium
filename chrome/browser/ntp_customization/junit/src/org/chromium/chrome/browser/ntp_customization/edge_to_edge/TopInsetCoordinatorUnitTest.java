// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.After;
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
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
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
    @Mock private View mView;
    @Mock private NativePage mNativePage;
    @Mock private TopInsetCoordinator.Observer mObserver;

    private final ObservableSupplierImpl<@Nullable Tab> mTabSupplier =
            new ObservableSupplierImpl<>();
    private NtpCustomizationConfigManager mNtpCustomizationConfigManager;
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

        mNtpCustomizationConfigManager = NtpCustomizationConfigManager.getInstance();
        mTopInsetCoordinator = new TopInsetCoordinator(mTabSupplier, mInsetObserver);
        mTopInsetCoordinator.addObserver(mObserver);

        mWindowInsetsCompat = createWindowInsetsCompat(TOP_PADDING);
    }

    @After
    public void tearDown() {
        mNtpCustomizationConfigManager.resetForTesting();
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnApplyWindowInsets_ConsumeTopInset() {
        Mockito.clearInvocations(mObserver);
        setCurrentTab(mNtpTab);

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
        setCurrentTab(mNonNtpTab1);

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
        setCurrentTab(mNtpTab);
        verify(mInsetObserver).retriggerOnApplyWindowInsets();

        // Updates |mConsumeTopInset| for the current NTP.
        mTopInsetCoordinator.onApplyWindowInsets(mView, mWindowInsetsCompat);
        // Verifies |mConsumeTopInset| is updated by setting to true, i.e, the current Tab is a NTP.
        assertTrue(mTopInsetCoordinator.getConsumeTopInsetForTesting());

        // Verifies that retriggerOnApplyWindowInsets() is called if the previous tab is a NTP.
        setCurrentTab(mNonNtpTab1);
        verify(mInsetObserver, times(2)).retriggerOnApplyWindowInsets();
        // Updates |mConsumeTopInset| for the current non NTP Tab.
        mTopInsetCoordinator.onApplyWindowInsets(mView, mWindowInsetsCompat);
        // Verifies |mConsumeTopInset| is updated by setting to false, i.e., the current Tab isn't a
        // NTP.
        assertFalse(mTopInsetCoordinator.getConsumeTopInsetForTesting());

        // Verifies that retriggerOnApplyWindowInsets() is NOT called if none of the new tab or the
        // previous tab is a NTP.
        Mockito.clearInvocations(mInsetObserver);
        setCurrentTab(mNonNtpTab2);
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
        setCurrentTab(mNonNtpTab1);
        mTopInsetCoordinator.destroy();

        verify(mInsetObserver).removeInsetsConsumer(any(InsetObserver.WindowInsetsConsumer.class));
        verify(mNonNtpTab1).removeObserver(any(TabObserver.class));
        assertEquals(0, mTopInsetCoordinator.getObserverCountForTesting());
    }

    @Test
    public void testOnBackgroundChanged() {
        Mockito.clearInvocations(mInsetObserver);
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        mNtpCustomizationConfigManager.notifyBackgroundImageChanged(
                bitmap, /* fromInitialization= */ true);
        verify(mInsetObserver, never()).retriggerOnApplyWindowInsets();

        mNtpCustomizationConfigManager.notifyBackgroundImageChanged(
                bitmap, /* fromInitialization= */ false);
        verify(mInsetObserver).retriggerOnApplyWindowInsets();
    }

    @Test
    public void testOnBackgroundColorChanged() {
        Mockito.clearInvocations(mInsetObserver);
        @ColorInt int color = Color.RED;

        mNtpCustomizationConfigManager.notifyBackgroundColorChanged(
                color, /* fromInitialization= */ true);
        assertEquals(color, mNtpCustomizationConfigManager.getBackgroundColorForTesting());
        verify(mInsetObserver, never()).retriggerOnApplyWindowInsets();

        mNtpCustomizationConfigManager.notifyBackgroundColorChanged(
                color, /* fromInitialization= */ false);
        assertEquals(color, mNtpCustomizationConfigManager.getBackgroundColorForTesting());
        verify(mInsetObserver).retriggerOnApplyWindowInsets();
    }

    @Test
    public void testObserveNotifyRefreshWindowInsets() {
        Mockito.clearInvocations(mInsetObserver);
        mNtpCustomizationConfigManager.notifyRefreshWindowInsets(/* consumeTopInset= */ true);
        verify(mInsetObserver).retriggerOnApplyWindowInsets();

        mNtpCustomizationConfigManager.notifyRefreshWindowInsets(/* consumeTopInset= */ false);
        verify(mInsetObserver, times(2)).retriggerOnApplyWindowInsets();
    }

    @Test
    public void testOnBackgroundChanged_AddAndRemoveObservers() {
        mTabSupplier.set(mNtpTab);
        verify(mNtpTab, never()).addObserver(any(TabObserver.class));

        // Verifies that observers are added when a customized background color is selected.
        mNtpCustomizationConfigManager.setBackgroundImageTypeFroTesting(
                NtpCustomizationUtils.NtpBackgroundImageType.CHROME_COLOR);
        mTopInsetCoordinator.onNtpBackgroundChanged();
        // Note: mTabSupplierObserver will add the first observer to mTrackingTab, and mTabObserver
        // will be added as the second observer to mTrackingTab.
        verify(mNtpTab, times(2)).addObserver(any(TabObserver.class));
        assertNotNull(mTopInsetCoordinator.getTabSupplierObserverForTesting());
        assertNotNull(mTopInsetCoordinator.getTrackingTabForTesting());

        // Verifies that observers are NOT added again when a customized background type is changed.
        Mockito.clearInvocations(mNtpTab);
        mNtpCustomizationConfigManager.setBackgroundImageTypeFroTesting(
                NtpCustomizationUtils.NtpBackgroundImageType.CHROME_THEME);
        mTopInsetCoordinator.onNtpBackgroundChanged();
        verify(mNtpTab, never()).addObserver(any(TabObserver.class));
        assertNotNull(mTopInsetCoordinator.getTabSupplierObserverForTesting());
        assertNotNull(mTopInsetCoordinator.getTrackingTabForTesting());

        // Verifies that observers are removed when the customized background is removed.
        mNtpCustomizationConfigManager.setBackgroundImageTypeFroTesting(
                NtpCustomizationUtils.NtpBackgroundImageType.DEFAULT);
        mTopInsetCoordinator.onNtpBackgroundChanged();
        verify(mNtpTab, times(2)).removeObserver(any(TabObserver.class));
        assertNull(mTopInsetCoordinator.getTabSupplierObserverForTesting());
        assertNull(mTopInsetCoordinator.getTrackingTabForTesting());
    }

    private WindowInsetsCompat createWindowInsetsCompat(int top) {
        Insets systemInsets = Insets.of(0, top, 0, 0);
        Insets displayCutoutInsets = Insets.of(0, top, 0, 0);
        var builder = new WindowInsetsCompat.Builder();
        return builder.setInsets(WindowInsetsCompat.Type.systemBars(), systemInsets)
                .setInsets(WindowInsetsCompat.Type.displayCutout(), displayCutoutInsets)
                .build();
    }

    private void setCurrentTab(Tab tab) {
        mTopInsetCoordinator.onTabSwitched(tab);
        mTabSupplier.set(tab);
    }
}
