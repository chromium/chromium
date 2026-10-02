// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Build;
import android.view.WindowManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.FilterLayoutStateObserver;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Unit tests for {@link IncognitoTabbedSnapshotController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class IncognitoTabbedSnapshotControllerTest {
    /** Robolectric does not shadow setRecentsScreenshotEnabled(), so record calls here. */
    private static class TestActivity extends Activity {
        private final List<Boolean> mRecentsScreenshotEnabledCalls = new ArrayList<>();

        @Override
        public void setRecentsScreenshotEnabled(boolean enabled) {
            mRecentsScreenshotEnabledCalls.add(enabled);
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabModelSelector mTabModelSelectorMock;
    @Mock private TabModel mTabModelMock;
    @Mock private TabModel mIncognitoTabModelMock;
    @Mock private LayoutManagerChrome mLayoutManagerMock;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcherMock;

    @Captor private ArgumentCaptor<LifecycleObserver> mLifecycleObserverArgumentCaptor;

    @Captor
    private ArgumentCaptor<FilterLayoutStateObserver> mFilterLayoutStateObserverArgumentCaptor;

    private TestActivity mActivity;
    private DestroyObserver mDestroyObserver;
    private FilterLayoutStateObserver mFilterLayoutStateObserver;
    private SettableMonotonicObservableSupplier<TabModel> mTabModelSupplier;

    private Supplier<Boolean> mIsIncognitoShowingSupplier;

    @Before
    public void before() {
        mTabModelSupplier = ObservableSuppliers.createMonotonic();
        when(mTabModelSelectorMock.getCurrentTabModelSupplier()).thenReturn(mTabModelSupplier);

        when(mTabModelSelectorMock.getModel(true)).thenReturn(mIncognitoTabModelMock);

        mIsIncognitoShowingSupplier =
                IncognitoTabbedSnapshotController.getIsShowingIncognitoSupplier(
                        mTabModelSelectorMock);

        mActivity = Robolectric.buildActivity(TestActivity.class).get();

        new IncognitoTabbedSnapshotController(
                mActivity,
                mLayoutManagerMock,
                mTabModelSelectorMock,
                mActivityLifecycleDispatcherMock,
                mIsIncognitoShowingSupplier);

        verify(mActivityLifecycleDispatcherMock, times(1))
                .register(mLifecycleObserverArgumentCaptor.capture());
        mDestroyObserver = (DestroyObserver) mLifecycleObserverArgumentCaptor.getValue();

        verify(mLayoutManagerMock, times(1))
                .addObserver(mFilterLayoutStateObserverArgumentCaptor.capture());
        mFilterLayoutStateObserver = mFilterLayoutStateObserverArgumentCaptor.getValue();
    }

    @Test
    @DisableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
    public void testSecureFlagsUnModified_ForIncognito_WhenAlreadyPresent() {
        mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        // In incognito
        when(mTabModelSelectorMock.getCurrentModel()).thenReturn(mTabModelMock);
        when(mTabModelMock.isIncognito()).thenReturn(true);

        mTabModelSupplier.set(mTabModelMock);

        assertTrue(isFlagSecureSet());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            assertEquals(List.of(), mActivity.mRecentsScreenshotEnabledCalls);
        }
    }

    @Test
    @DisableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
    public void testSecureFlagsAdded_ForIncognito_WhenNotAlreadyPresent() {
        mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_SECURE);

        // In incognito
        when(mTabModelSelectorMock.getCurrentModel()).thenReturn(mTabModelMock);
        when(mTabModelMock.isIncognito()).thenReturn(true);

        mTabModelSupplier.set(mTabModelMock);

        assertTrue(isFlagSecureSet());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            assertEquals(List.of(), mActivity.mRecentsScreenshotEnabledCalls);
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testFlagSecureCleared_ForIncognito_WhenIncognitoScreenshotEnabled() {
        mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        // In incognito
        when(mTabModelSelectorMock.getCurrentModel()).thenReturn(mTabModelMock);
        when(mTabModelMock.isIncognito()).thenReturn(true);

        mTabModelSupplier.set(mTabModelMock);

        assertFalse(isFlagSecureSet());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            assertEquals(List.of(false), mActivity.mRecentsScreenshotEnabledCalls);
        }
    }

    @Test
    @DisableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
    public void testFlagSecureCleared_AfterSwitchingToNonIncognito_WithScreenshotDisabled() {
        mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);

        // In regular mode.
        when(mTabModelSelectorMock.getCurrentModel()).thenReturn(mTabModelMock);
        when(mTabModelMock.isIncognito()).thenReturn(false);

        mTabModelSupplier.set(mTabModelMock);

        assertFalse(isFlagSecureSet());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            assertEquals(List.of(), mActivity.mRecentsScreenshotEnabledCalls);
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testFlagSecureCleared_AfterSwitchingToNonIncognito_ScreenshotEnabled() {
        mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);

        // In regular mode.
        when(mTabModelSelectorMock.getCurrentModel()).thenReturn(mTabModelMock);
        when(mTabModelMock.isIncognito()).thenReturn(false);

        mTabModelSupplier.set(mTabModelMock);

        assertFalse(isFlagSecureSet());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            assertEquals(List.of(true), mActivity.mRecentsScreenshotEnabledCalls);
        }
    }

    @Test
    public void testIsShowingIncognito_CurrentModelRegular_ReturnsFalse() {
        // Regular mode
        when(mTabModelSelectorMock.getCurrentModel()).thenReturn(mTabModelMock);
        when(mTabModelMock.isIncognito()).thenReturn(false);

        assertFalse("isShowingIncognito should return false ", mIsIncognitoShowingSupplier.get());
    }

    @Test
    public void testIsShowingIncognito_CurrentModelIncognito_ReturnsTrue() {
        when(mTabModelSelectorMock.getCurrentModel()).thenReturn(mTabModelMock);
        when(mTabModelMock.isIncognito()).thenReturn(true);

        assertTrue("isShowingIncognito should be true", mIsIncognitoShowingSupplier.get());

        verify(mTabModelSelectorMock, never()).getModel(true);
    }

    @Test
    public void testOnDestroy_PerformsCleanUp() {
        mDestroyObserver.onDestroy();
        verify(mLayoutManagerMock, times(1)).removeObserver(mFilterLayoutStateObserver);
        assertFalse(mTabModelSupplier.hasObservers());
        verify(mActivityLifecycleDispatcherMock, times(1)).unregister(mDestroyObserver);
    }

    private boolean isFlagSecureSet() {
        return (mActivity.getWindow().getAttributes().flags
                        & WindowManager.LayoutParams.FLAG_SECURE)
                != 0;
    }
}
