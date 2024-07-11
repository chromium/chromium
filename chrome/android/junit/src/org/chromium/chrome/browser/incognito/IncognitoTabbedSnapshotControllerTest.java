// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.os.Build;
import android.view.Window;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
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
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;

/** Unit tests for {@link IncognitoTabbedSnapshotController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class IncognitoTabbedSnapshotControllerTest {
    @Mock private Window mWindowMock;
    @Mock private Activity mActivityMock;
    @Mock private TabModelSelector mTabModelSelectorMock;
    @Mock private TabModel mTabModelMock;
    @Mock private TabModel mIncognitoTabModelMock;
    @Mock private LayoutManagerChrome mLayoutManagerMock;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcherMock;

    @Captor private ArgumentCaptor<LifecycleObserver> mLifecycleObserverArgumentCaptor;

    @Captor
    private ArgumentCaptor<FilterLayoutStateObserver> mFilterLayoutStateObserverArgumentCaptor;

    @Captor
    private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverArgumentCaptor;

    private IncognitoTabbedSnapshotController mController;
    private WindowManager.LayoutParams mParams;
    private DestroyObserver mDestroyObserver;
    private FilterLayoutStateObserver mFilterLayoutStateObserver;
    private TabModelSelectorObserver mTabModelSelectorObserver;
    private boolean mIsInOverviewMode;

    private Supplier<Boolean> mIsIncognitoShowingSupplier;
    private Supplier<Boolean> mIsInOverviewModeSupplier;

    @Before
    public void before() {
        MockitoAnnotations.initMocks(this);
        doReturn(mIncognitoTabModelMock)
                .when(mTabModelSelectorMock)
                .getModel(/* incognito= */ true);

        mIsInOverviewModeSupplier = () -> mIsInOverviewMode;

        mIsIncognitoShowingSupplier =
                IncognitoTabbedSnapshotController.getIsShowingIncognitoSupplier(
                        mTabModelSelectorMock, mIsInOverviewModeSupplier);

        mParams = new LayoutParams();
        doReturn(mParams).when(mWindowMock).getAttributes();
        doReturn(mWindowMock).when(mActivityMock).getWindow();

        mController =
                new IncognitoTabbedSnapshotController(
                        mActivityMock,
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

        verify(mTabModelSelectorMock, times(1))
                .addObserver(mTabModelSelectorObserverArgumentCaptor.capture());
        mTabModelSelectorObserver = mTabModelSelectorObserverArgumentCaptor.getValue();
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
    public void testSecureFlagsUnModified_ForIncognito_WhenAlreadyPresent() {
        mParams.flags = WindowManager.LayoutParams.FLAG_SECURE;
        // In incognito
        doReturn(mTabModelMock).when(mTabModelSelectorMock).getCurrentModel();
        doReturn(true).when(mTabModelMock).isIncognito();

        mTabModelSelectorObserver.onChange();

        verify(mWindowMock, never()).addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        verify(mWindowMock, never()).clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            verify(mActivityMock, never()).setRecentsScreenshotEnabled(anyBoolean());
        }
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
    public void testSecureFlagsAdded_ForIncognito_WhenNotAlreadyPresent() {
        mParams.flags = 0;

        // In incognito
        doReturn(mTabModelMock).when(mTabModelSelectorMock).getCurrentModel();
        doReturn(true).when(mTabModelMock).isIncognito();

        mTabModelSelectorObserver.onChange();

        verify(mWindowMock, times(1)).addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            verify(mActivityMock, times(1)).setRecentsScreenshotEnabled(false);
        }
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testFlagSecureCleared_ForIncognito_WhenIncognitoScreenshotEnabled() {
        mParams.flags = WindowManager.LayoutParams.FLAG_SECURE;
        // In incognito
        doReturn(mTabModelMock).when(mTabModelSelectorMock).getCurrentModel();
        doReturn(true).when(mTabModelMock).isIncognito();

        mTabModelSelectorObserver.onChange();

        verify(mWindowMock, times(1)).clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            verify(mActivityMock, never()).setRecentsScreenshotEnabled(anyBoolean());
        }
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
    public void testFlagSecureCleared_AfterSwitchingToNonIncognito_WithScreenshotDisabled() {
        mParams.flags = WindowManager.LayoutParams.FLAG_SECURE;

        // In regular mode.
        doReturn(mTabModelMock).when(mTabModelSelectorMock).getCurrentModel();
        doReturn(false).when(mTabModelMock).isIncognito();

        mTabModelSelectorObserver.onChange();

        verify(mWindowMock, times(1)).clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            verify(mActivityMock, never()).setRecentsScreenshotEnabled(anyBoolean());
        }
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.INCOGNITO_SCREENSHOT)
    public void testFlagSecureCleared_AfterSwitchingToNonIncognito_ScreenshotEnabled() {
        mParams.flags = WindowManager.LayoutParams.FLAG_SECURE;

        // In regular mode.
        doReturn(mTabModelMock).when(mTabModelSelectorMock).getCurrentModel();
        doReturn(false).when(mTabModelMock).isIncognito();

        mTabModelSelectorObserver.onChange();

        verify(mWindowMock, times(1)).clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            verify(mActivityMock, times(1)).setRecentsScreenshotEnabled(true);
        }
    }

    @Test
    @SmallTest
    public void testIsShowingIncognito_CurrentModelRegular_ReturnsFalse() {
        // Regular mode
        doReturn(mTabModelMock).when(mTabModelSelectorMock).getCurrentModel();
        doReturn(false).when(mTabModelMock).isIncognito();

        assertFalse("isShowingIncognito should return false ", mIsIncognitoShowingSupplier.get());
    }

    @Test
    @SmallTest
    public void testIsShowingIncognito_CurrentModelIncognito_ReturnsTrue() {
        doReturn(mTabModelMock).when(mTabModelSelectorMock).getCurrentModel();
        doReturn(true).when(mTabModelMock).isIncognito();

        assertTrue("isShowingIncognito should be true", mIsIncognitoShowingSupplier.get());

        verify(mTabModelSelectorMock, never()).getModel(true);
    }

    @Test
    @SmallTest
    public void testOnDestroy_PerformsCleanUp() {
        mDestroyObserver.onDestroy();
        verify(mLayoutManagerMock, times(1)).removeObserver(mFilterLayoutStateObserver);
        verify(mTabModelSelectorMock, times(1)).removeObserver(mTabModelSelectorObserver);
        verify(mActivityLifecycleDispatcherMock, times(1)).unregister(mDestroyObserver);
    }
}
