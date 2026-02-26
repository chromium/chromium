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
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Point;
import android.view.View;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorFromHexInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.TopInsetProvider;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TopInsetCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
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
    @Mock private TopInsetProvider.Observer mObserver;
    @Mock private LayoutStateProvider mLayoutStateProvider;

    @Captor
    private ArgumentCaptor<LayoutStateProvider.LayoutStateObserver> mLayoutStateObserverCaptor;

    private final SettableNullableObservableSupplier<Tab> mTabSupplier =
            ObservableSuppliers.createNullable();
    private final OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();

    private Context mContext;
    private NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    private TopInsetCoordinator mTopInsetCoordinator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);

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

        mNtpCustomizationConfigManager = new NtpCustomizationConfigManager();
        NtpCustomizationConfigManager.setInstanceForTesting(mNtpCustomizationConfigManager);
        mTopInsetCoordinator =
                new TopInsetCoordinator(
                        mContext, mTabSupplier, mInsetObserver, mLayoutStateProviderSupplier);
        mTopInsetCoordinator.addObserver(mObserver);

        mWindowInsetsCompat = createWindowInsetsCompat(TOP_PADDING);
        clearInvocations(mNtpTab);
        clearInvocations(mInsetObserver);
    }

    @After
    public void tearDown() {
        mNtpCustomizationConfigManager.resetForTesting();
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        mTopInsetCoordinator.destroy();
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnApplyWindowInsets_ConsumeTopInset() {
        clearInvocations(mObserver);
        setCurrentTab(mNtpTab);

        assertNotNull(mWindowInsetsCompat.getInsets(WindowInsetsCompat.Type.systemBars()));
        assertNotNull(mWindowInsetsCompat.getInsets(WindowInsetsCompat.Type.displayCutout()));
        WindowInsetsCompat result =
                mTopInsetCoordinator.onApplyWindowInsets(mView, mWindowInsetsCompat);

        // Verify that the top inset is consumed for NTP.
        assertNotEquals(mWindowInsetsCompat, result);
        assertEquals(Insets.NONE, result.getInsets(WindowInsetsCompat.Type.systemBars()));
        assertEquals(Insets.NONE, result.getInsets(WindowInsetsCompat.Type.displayCutout()));
        verify(mObserver).onToEdgeChange(eq(TOP_PADDING), eq(true), anyInt());
    }

    @Test
    public void testOnApplyWindowInsets_DoNotConsumeTopInset() {
        when(mNativePage.supportsEdgeToEdgeOnTop()).thenReturn(false);
        clearInvocations(mObserver);
        setCurrentTab(mNonNtpTab1);

        WindowInsetsCompat result =
                mTopInsetCoordinator.onApplyWindowInsets(mView, mWindowInsetsCompat);

        // Verify that the top inset is not consumed for non-NTP tab.
        assertEquals(mWindowInsetsCompat, result);
        verify(mObserver).onToEdgeChange(eq(TOP_PADDING), eq(false), anyInt());
    }

    @Test
    public void testOnApplyWindowInsets_ToolbarSwipe() {
        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        setBackgroundType(NtpBackgroundType.DEFAULT, NtpBackgroundType.CHROME_COLOR);

        when(mLayoutStateProvider.getActiveLayoutType()).thenReturn(LayoutType.TOOLBAR_SWIPE);
        mTabSupplier.set(null);
        clearInvocations(mObserver);

        mTopInsetCoordinator.onApplyWindowInsets(mView, mWindowInsetsCompat);

        // Verify that notifyObservers() is called because it's a toolbar swipe.
        verify(mObserver).onToEdgeChange(eq(TOP_PADDING), eq(false), anyInt());
    }

    @Test
    public void testOnTabSwitched_RetriggerOnApplyWindowInsets() {
        // Verifies that retriggerOnApplyWindowInsets() is called if the new tab is a NTP.
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
        clearInvocations(mInsetObserver);
        setCurrentTab(mNonNtpTab2);
        verify(mInsetObserver, never()).retriggerOnApplyWindowInsets();
    }

    @Test
    public void testOnTabSwitched_NullTab() {
        mTopInsetCoordinator.onTabSwitched(null);
        verify(mInsetObserver, never()).retriggerOnApplyWindowInsets();
    }

    @Test
    public void testDestroy() {
        clearInvocations(mLayoutStateProvider);
        setCurrentTab(mNonNtpTab1);
        // Add observer to the mLayoutStateProvider.
        mLayoutStateProviderSupplier.set(mLayoutStateProvider);

        setBackgroundType(NtpBackgroundType.DEFAULT, NtpBackgroundType.CHROME_COLOR);
        verify(mLayoutStateProvider)
                .addObserver(any(LayoutStateProvider.LayoutStateObserver.class));

        clearInvocations(mNonNtpTab1);
        mTopInsetCoordinator.destroy();

        verify(mInsetObserver).removeInsetsConsumer(any(InsetObserver.WindowInsetsConsumer.class));
        // Note: mTabSupplierObserver will add the first observer to mTrackingTab, and mTabObserver
        // will be added as the second observer to mTrackingTab.
        verify(mNonNtpTab1, times(2)).removeObserver(any(TabObserver.class));
        verify(mLayoutStateProvider)
                .removeObserver(any(LayoutStateProvider.LayoutStateObserver.class));
        assertEquals(0, mTopInsetCoordinator.getObserverCountForTesting());
    }

    @Test
    public void testOnBackgroundChanged_fromInitialization() {
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        Matrix portraitMatrix = new Matrix();
        Matrix landscapeMatrix = new Matrix();
        landscapeMatrix.setScale(2f, 9f);
        BackgroundImageInfo imageInfo =
                new BackgroundImageInfo(portraitMatrix, landscapeMatrix, new Point(), new Point());

        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(
                NtpBackgroundType.IMAGE_FROM_DISK);
        mNtpCustomizationConfigManager.notifyBackgroundImageChanged(
                bitmap,
                imageInfo,
                /* fromInitialization= */ true,
                /* oldType= */ NtpBackgroundType.DEFAULT);
        verify(mInsetObserver, never()).retriggerOnApplyWindowInsets();

        mNtpCustomizationConfigManager.notifyBackgroundImageChanged(
                bitmap,
                imageInfo,
                /* fromInitialization= */ false,
                /* oldType= */ NtpBackgroundType.DEFAULT);
        verify(mInsetObserver).retriggerOnApplyWindowInsets();
    }

    @Test
    public void testOnBackgroundColorChanged_fromInitialization() {
        NtpThemeColorFromHexInfo colorInfo =
                new NtpThemeColorFromHexInfo(mContext, Color.RED, NtpThemeColorInfo.COLOR_NOT_SET);

        mNtpCustomizationConfigManager.setNtpThemeColorInfoForTesting(colorInfo);
        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(NtpBackgroundType.CHROME_COLOR);
        mNtpCustomizationConfigManager.notifyBackgroundColorChanged(
                mContext, /* fromInitialization= */ true, NtpBackgroundType.DEFAULT);
        assertEquals(colorInfo, mNtpCustomizationConfigManager.getNtpThemeColorInfo());
        verify(mInsetObserver, never()).retriggerOnApplyWindowInsets();

        mNtpCustomizationConfigManager.notifyBackgroundColorChanged(
                mContext, /* fromInitialization= */ false, NtpBackgroundType.DEFAULT);
        assertEquals(colorInfo, mNtpCustomizationConfigManager.getNtpThemeColorInfo());
        verify(mInsetObserver).retriggerOnApplyWindowInsets();
    }

    @Test
    public void testOnBackgroundChanged_addAndRemoveObservers() {
        mTabSupplier.set(mNtpTab);
        clearInvocations(mLayoutStateProvider);
        mLayoutStateProviderSupplier.set(mLayoutStateProvider);

        verify(mLayoutStateProvider, never())
                .addObserver(any(LayoutStateProvider.LayoutStateObserver.class));

        clearInvocations(mNtpTab);
        clearInvocations(mInsetObserver);

        // Verifies that observers are added when a customized background color is selected.
        setBackgroundType(NtpBackgroundType.DEFAULT, NtpBackgroundType.CHROME_COLOR);
        // Note: mTabSupplierObserver will add the first observer to mTrackingTab, and mTabObserver
        // will be added as the second observer to mTrackingTab.
        verify(mNtpTab, times(2)).addObserver(any(TabObserver.class));
        verify(mLayoutStateProvider)
                .addObserver(any(LayoutStateProvider.LayoutStateObserver.class));
        assertNotNull(mTopInsetCoordinator.getTabSupplierObserverForTesting());
        assertNotNull(mTopInsetCoordinator.getTrackingTabForTesting());

        // Verifies that observers are NOT added again when a customized background type is changed.
        clearInvocations(mNtpTab);
        clearInvocations(mLayoutStateProvider);
        setBackgroundType(NtpBackgroundType.CHROME_COLOR, NtpBackgroundType.THEME_COLLECTION);
        verify(mNtpTab, never()).addObserver(any(TabObserver.class));
        verify(mLayoutStateProvider, never())
                .addObserver(any(LayoutStateProvider.LayoutStateObserver.class));
        assertNotNull(mTopInsetCoordinator.getTabSupplierObserverForTesting());
        assertNotNull(mTopInsetCoordinator.getTrackingTabForTesting());

        // Verifies that observers are removed when the customized background is removed.
        resetBackgroundType(NtpBackgroundType.THEME_COLLECTION);
        verify(mNtpTab, times(2)).removeObserver(any(TabObserver.class));
        verify(mLayoutStateProvider)
                .removeObserver(any(LayoutStateProvider.LayoutStateObserver.class));
        assertNull(mTopInsetCoordinator.getTabSupplierObserverForTesting());
        assertNull(mTopInsetCoordinator.getTrackingTabForTesting());

        // Verifies it is no-op when the background type is set to the default one again.
        clearInvocations(mNtpTab);
        clearInvocations(mLayoutStateProvider);
        resetBackgroundType(NtpBackgroundType.DEFAULT);
        verify(mNtpTab, never()).removeObserver(any(TabObserver.class));
        verify(mLayoutStateProvider, never())
                .removeObserver(any(LayoutStateProvider.LayoutStateObserver.class));
        assertNull(mTopInsetCoordinator.getTabSupplierObserverForTesting());
        assertNull(mTopInsetCoordinator.getTrackingTabForTesting());
    }

    @Test
    public void testOnBackgroundChanged_RefreshWindowInsets() {
        mTabSupplier.set(mNtpTab);
        clearInvocations(mNtpTab);
        clearInvocations(mInsetObserver);

        // Verifies that retriggerOnApplyWindowInsets() is called when a customized background color
        // is selected.
        setBackgroundType(NtpBackgroundType.DEFAULT, NtpBackgroundType.CHROME_COLOR);
        verify(mInsetObserver).retriggerOnApplyWindowInsets();

        // Verifies that retriggerOnApplyWindowInsets() isn't called again when the customized
        // background type is changed.
        clearInvocations(mInsetObserver);
        setBackgroundType(NtpBackgroundType.CHROME_COLOR, NtpBackgroundType.THEME_COLLECTION);
        verify(mInsetObserver, never()).retriggerOnApplyWindowInsets();

        // Verifies that retriggerOnApplyWindowInsets() is called when the customized background is
        // removed.
        resetBackgroundType(NtpBackgroundType.THEME_COLLECTION);
        verify(mInsetObserver).retriggerOnApplyWindowInsets();

        // Verifies that retriggerOnApplyWindowInsets() isn't called again when the background type
        // is set to default again.
        clearInvocations(mInsetObserver);
        setBackgroundType(NtpBackgroundType.DEFAULT, NtpBackgroundType.DEFAULT);
        verify(mInsetObserver, never()).retriggerOnApplyWindowInsets();
    }

    @Test
    public void testOnLayoutStateProviderAvailable_notAvailableBeforeChangingBackgroundType() {
        // Tests the case that LayoutStateProvider hasn't been initialized before the background
        // type of NTP is changed.
        assertNull(mLayoutStateProviderSupplier.get());
        clearInvocations(mLayoutStateProvider);

        setBackgroundType(NtpBackgroundType.DEFAULT, NtpBackgroundType.CHROME_COLOR);
        verify(mLayoutStateProvider, never())
                .addObserver(any(LayoutStateProvider.LayoutStateObserver.class));
        // Verifies mAddLayoutStateObserverPending is set to true.
        assertTrue(mTopInsetCoordinator.getAddLayoutStateObserverPendingForTesting());

        // Verifies the observer is added when the LayoutStateProvider is available.
        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mLayoutStateProvider)
                .addObserver(any(LayoutStateProvider.LayoutStateObserver.class));
        assertFalse(mTopInsetCoordinator.getAddLayoutStateObserverPendingForTesting());
    }

    @Test
    public void testOnLayoutStateProviderAvailable_availableBeforeChangingBackgroundType() {
        clearInvocations(mLayoutStateProvider);
        mLayoutStateProviderSupplier.set(mLayoutStateProvider);

        // Verifies that observer isn't added when the background type is still default.
        verify(mLayoutStateProvider, never())
                .addObserver(any(LayoutStateProvider.LayoutStateObserver.class));
        assertFalse(mTopInsetCoordinator.getAddLayoutStateObserverPendingForTesting());

        // Verifies that observer is added when a customized background type is set.
        setBackgroundType(NtpBackgroundType.DEFAULT, NtpBackgroundType.CHROME_COLOR);
        verify(mLayoutStateProvider)
                .addObserver(any(LayoutStateProvider.LayoutStateObserver.class));
        assertFalse(mTopInsetCoordinator.getAddLayoutStateObserverPendingForTesting());
    }

    @Test
    public void testOnFinishShowing_betweenTabSwitcherAndNtp() {
        clearInvocations(mLayoutStateProvider);
        mLayoutStateProviderSupplier.set(mLayoutStateProvider);

        setBackgroundType(NtpBackgroundType.DEFAULT, NtpBackgroundType.CHROME_COLOR);
        verify(mLayoutStateProvider).addObserver(mLayoutStateObserverCaptor.capture());

        // 1. Tests the transition from Tab switcher to a NTP
        // Switches to Tab switcher.
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.TAB_SWITCHER);
        assertTrue(mTopInsetCoordinator.getIsTabSwitcherShowingForTesting());

        // Sets the next Tab is NTP.
        mTopInsetCoordinator.onTabSwitched(mNtpTab);
        assertTrue(mTopInsetCoordinator.getInTabSwitcherToNtpTransitionForTesting());
        clearInvocations(mInsetObserver);

        // Finishes hiding the Tab switcher.
        mLayoutStateObserverCaptor.getValue().onFinishedHiding(LayoutType.TAB_SWITCHER);
        assertFalse(mTopInsetCoordinator.getInTabSwitcherToNtpTransitionForTesting());
        verify(mInsetObserver).retriggerOnApplyWindowInsets();

        // 2. Tests the transition from a NTP to Tab switcher.
        // Switches to Tab switcher.
        mTopInsetCoordinator.onTabSwitched(null);
        assertFalse(mTopInsetCoordinator.getInTabSwitcherToNtpTransitionForTesting());
        clearInvocations(mInsetObserver);
        // Finishes hiding the NTP.
        mLayoutStateObserverCaptor.getValue().onFinishedHiding(LayoutType.BROWSING);
        assertTrue(mTopInsetCoordinator.getIsTabSwitcherShowingForTesting());
        verify(mInsetObserver, never()).retriggerOnApplyWindowInsets();
    }

    private WindowInsetsCompat createWindowInsetsCompat(int top) {
        Insets systemInsets = Insets.of(0, top, 0, 0);
        Insets displayCutoutInsets = Insets.of(0, top, 0, 0);
        var builder = new WindowInsetsCompat.Builder();
        return builder.setInsets(WindowInsetsCompat.Type.statusBars(), systemInsets)
                .setInsets(WindowInsetsCompat.Type.captionBar(), systemInsets)
                .setInsets(WindowInsetsCompat.Type.displayCutout(), displayCutoutInsets)
                .build();
    }

    private void setCurrentTab(Tab tab) {
        mTopInsetCoordinator.onTabSwitched(tab);
        mTabSupplier.set(tab);
    }

    private void setBackgroundType(@NtpBackgroundType int oldType, @NtpBackgroundType int newType) {
        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(newType);
        mTopInsetCoordinator.onNtpBackgroundChanged(
                /* fromInitialization= */ false, oldType, newType);
    }

    private void resetBackgroundType(@NtpBackgroundType int oldType) {
        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(NtpBackgroundType.DEFAULT);
        mTopInsetCoordinator.onNtpBackgroundReset(oldType);
    }
}
