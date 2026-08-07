// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Unit tests for {@link TabBottomSheetSuppressionController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabBottomSheetSuppressionControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private TabBottomSheetSuppressionController.Delegate mDelegate;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab;
    @Mock private ManualFillingComponent mManualFillingComponent;
    @Mock private NavigationHandle mNavigationHandle;

    @Captor private ArgumentCaptor<LayoutStateObserver> mLayoutStateObserverCaptor;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private final OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();
    private final SettableNonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier =
            ObservableSuppliers.createNonNull(false);
    private final SettableMonotonicObservableSupplier<TabModel> mTabModelSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNullableObservableSupplier<Tab> mCurrentTabSupplier =
            ObservableSuppliers.createNullable();
    private final SettableMonotonicObservableSupplier<ManualFillingComponent>
            mManualFillingComponentSupplier = ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<Boolean> mIsAccessoryRequestedSupplier =
            ObservableSuppliers.createNonNull(false);
    private final SettableNullableObservableSupplier<Tab> mActivePlaybackTabSupplier =
            ObservableSuppliers.createNullable();

    private TabBottomSheetSuppressionController mController;

    @Before
    public void setUp() {
        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);

        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mTabModelSupplier);
        when(mTabModelSelector.getCurrentTabSupplier()).thenReturn(mCurrentTabSupplier);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTabModel.isIncognito()).thenReturn(false);

        when(mManualFillingComponent.getIsAccessoryRequestedSupplier())
                .thenReturn(mIsAccessoryRequestedSupplier);

        mLayoutStateProviderSupplier.set(mLayoutStateProvider);

        mController =
                new TabBottomSheetSuppressionController(
                        mWindowAndroid,
                        mLayoutStateProviderSupplier,
                        mOmniboxFocusStateSupplier,
                        mDelegate);

        mController.setManualFillingComponentSupplierForTesting(mManualFillingComponentSupplier);
        mController.initReadAloudIntegrationForTesting(mActivePlaybackTabSupplier, () -> {});

        ShadowLooper.idleMainLooper();
        verify(mLayoutStateProvider).addObserver(mLayoutStateObserverCaptor.capture());
        clearInvocations(mDelegate);
    }

    @Test
    public void testOmniboxFocus_suppresses() {
        assertFalse(mController.isInternallySuppressed());

        mOmniboxFocusStateSupplier.set(true);
        assertTrue(mController.isInternallySuppressed());
        verify(mDelegate).onSuppressionStarted();

        mOmniboxFocusStateSupplier.set(false);
        assertFalse(mController.isInternallySuppressed());
        verify(mDelegate).onSuppressionEnded();
    }

    @Test
    public void testLayoutState_suppresses() {
        LayoutStateObserver observer = mLayoutStateObserverCaptor.getValue();
        assertFalse(mController.isInternallySuppressed());

        observer.onStartedShowing(LayoutType.HUB);
        assertTrue(mController.isInternallySuppressed());
        verify(mDelegate).onSuppressionStarted();

        when(mLayoutStateProvider.getNextLayoutType()).thenReturn(LayoutType.BROWSING);
        observer.onStartedHiding(LayoutType.HUB);
        assertFalse(mController.isInternallySuppressed());
        verify(mDelegate).onSuppressionEnded();
    }

    @Test
    public void testLayoutState_swipeSuppresses() {
        LayoutStateObserver observer = mLayoutStateObserverCaptor.getValue();
        assertFalse(mController.isInternallySuppressed());

        observer.onStartedShowing(LayoutType.TOOLBAR_SWIPE);
        assertTrue(mController.isInternallySuppressed());
        verify(mDelegate).onSuppressionStarted();

        when(mLayoutStateProvider.getNextLayoutType()).thenReturn(LayoutType.BROWSING);
        observer.onStartedHiding(LayoutType.TOOLBAR_SWIPE);
        assertFalse(mController.isInternallySuppressed());
        verify(mDelegate).onSuppressionEnded();
    }

    @Test
    public void testTabModel_incognitoSuppresses() {
        when(mTabModel.isIncognito()).thenReturn(true);
        mTabModelSupplier.set(mTabModel);

        assertTrue(mController.isInternallySuppressed());
        verify(mDelegate).onSuppressionStarted();

        TabModel normalModel = mock(TabModel.class);
        when(normalModel.isIncognito()).thenReturn(false);
        mTabModelSupplier.set(normalModel);

        assertFalse(mController.isInternallySuppressed());
        verify(mDelegate).onSuppressionEnded();
    }

    @Test
    public void testManualFillingComponent_suppresses() {
        mManualFillingComponentSupplier.set(mManualFillingComponent);

        mIsAccessoryRequestedSupplier.set(true);
        assertTrue(mController.isInternallySuppressed());
        verify(mDelegate).onSuppressionStarted();

        mIsAccessoryRequestedSupplier.set(false);
        assertFalse(mController.isInternallySuppressed());
        verify(mDelegate).onSuppressionEnded();
    }

    @Test
    public void testReadAloud_suppresses() {
        mActivePlaybackTabSupplier.set(mTab);
        assertTrue(mController.isInternallySuppressed());
        verify(mDelegate).onSuppressionStarted();

        mActivePlaybackTabSupplier.set(null);
        assertFalse(mController.isInternallySuppressed());
        verify(mDelegate).onSuppressionEnded();
    }

    @Test
    public void testHandleReadAloudStopPlayback() {
        Runnable stopCallback = mock(Runnable.class);
        mController.initReadAloudIntegrationForTesting(mActivePlaybackTabSupplier, stopCallback);

        // When not suppressed by read aloud, callback is not run.
        mController.handleReadAloudStopPlayback();
        verify(stopCallback, never()).run();

        // Suppress by read aloud.
        mActivePlaybackTabSupplier.set(mTab);
        mController.handleReadAloudStopPlayback();
        verify(stopCallback).run();
    }

    @Test
    public void testTabObserver_closeRequestedForDistiller() {
        mCurrentTabSupplier.set(mTab);
        verify(mTab).addObserver(mTabObserverCaptor.capture());

        GURL distillerUrl = new GURL(UrlConstants.DISTILLER_SCHEME + "://foo");
        when(mNavigationHandle.getUrl()).thenReturn(distillerUrl);

        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(mTab, mNavigationHandle);

        verify(mDelegate).onCloseRequested();
    }

    @Test
    public void testDestroy() {
        mController.destroy();
        assertFalse(mOmniboxFocusStateSupplier.hasObservers());
        assertFalse(mActivePlaybackTabSupplier.hasObservers());
        assertFalse(mManualFillingComponentSupplier.hasObservers());
        verify(mLayoutStateProvider).removeObserver(any());
    }
}
