// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.scroll_capture;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;

/** Tests for the ScreenshotBoundsManager */
@RunWith(BaseRobolectricTestRunner.class)
public class ScrollCaptureManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Tab mTab;
    @Mock private ScrollCaptureManagerDelegate mScrollCaptureManagerDelegateMock;
    @Mock private Tab mTab1;
    @Mock private View mView;
    @Mock private View mAnotherView;

    private SettableNullableObservableSupplier<Tab> mTabSupplier;
    private ScrollCaptureManager mScrollCaptureManager;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabSupplier = ObservableSuppliers.createNullable();
                    mScrollCaptureManager =
                            new ScrollCaptureManager(
                                    mTabSupplier, mScrollCaptureManagerDelegateMock);
                });
    }

    @Test
    public void testObserveTab() {
        InOrder inOrder = Mockito.inOrder(mTab, mTab1, mScrollCaptureManagerDelegateMock);

        mTabSupplier.set(mTab);
        inOrder.verify(mScrollCaptureManagerDelegateMock).setCurrentTab(mTab);
        inOrder.verify(mTab).addObserver(mScrollCaptureManager);
        inOrder.verify(mTab).getView();

        mTabSupplier.set(mTab1);
        inOrder.verify(mTab).removeObserver(mScrollCaptureManager);
        inOrder.verify(mScrollCaptureManagerDelegateMock).setCurrentTab(mTab1);
        inOrder.verify(mTab1).addObserver(mScrollCaptureManager);
        inOrder.verify(mTab1).getView();
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void testContentChange() {
        InOrder inOrder = Mockito.inOrder(mScrollCaptureManagerDelegateMock);

        // No view available
        mScrollCaptureManager.onContentChanged(mTab);

        // View is set
        when(mTab.getView()).thenReturn(mView);
        mScrollCaptureManager.onContentChanged(mTab);
        inOrder.verify(mScrollCaptureManagerDelegateMock).addScrollCaptureBindings(eq(mView));

        // Content change
        when(mTab.getView()).thenReturn(mAnotherView);
        mScrollCaptureManager.onContentChanged(mTab);
        inOrder.verify(mScrollCaptureManagerDelegateMock).removeScrollCaptureBindings(eq(mView));
        inOrder.verify(mScrollCaptureManagerDelegateMock)
                .addScrollCaptureBindings(eq(mAnotherView));

        // Test when native page
        when(mTab.isNativePage()).thenReturn(true);
        mScrollCaptureManager.onContentChanged(mTab);
        inOrder.verify(mScrollCaptureManagerDelegateMock)
                .removeScrollCaptureBindings(eq(mAnotherView));
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void testDestroy() {
        InOrder inOrder = Mockito.inOrder(mTab, mScrollCaptureManagerDelegateMock);

        when(mTab.getView()).thenReturn(mView);
        mTabSupplier.set(mTab);
        mScrollCaptureManager.destroy();
        inOrder.verify(mTab).removeObserver(mScrollCaptureManager);
        inOrder.verify(mScrollCaptureManagerDelegateMock).removeScrollCaptureBindings(eq(mView));
    }
}
