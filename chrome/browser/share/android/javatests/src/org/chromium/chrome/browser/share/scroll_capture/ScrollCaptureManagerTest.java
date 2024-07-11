// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.scroll_capture;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.test.filters.SmallTest;

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
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.tab.Tab;

/** Tests for the ScreenshotBoundsManager */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ScrollCaptureManagerTest {
    @Mock private Tab mTab;
    @Mock private ScrollCaptureManagerDelegate mScrollCaptureManagerDelegateMock;

    private ObservableSupplierImpl<Tab> mTabSupplier;
    private ScrollCaptureManager mScrollCaptureManager;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabSupplier = new ObservableSupplierImpl<>();
                    mScrollCaptureManager =
                            new ScrollCaptureManager(
                                    mTabSupplier, mScrollCaptureManagerDelegateMock);
                });
    }

    @Test
    @SmallTest
    public void testObserveTab() {
        Tab tab = mock(Tab.class);
        InOrder inOrder = Mockito.inOrder(mTab, tab, mScrollCaptureManagerDelegateMock);

        mTabSupplier.set(mTab);
        inOrder.verify(mScrollCaptureManagerDelegateMock).setCurrentTab(mTab);
        inOrder.verify(mTab).addObserver(mScrollCaptureManager);
        inOrder.verify(mTab).getView();

        mTabSupplier.set(tab);
        inOrder.verify(mTab).removeObserver(mScrollCaptureManager);
        inOrder.verify(mScrollCaptureManagerDelegateMock).setCurrentTab(tab);
        inOrder.verify(tab).addObserver(mScrollCaptureManager);
        inOrder.verify(tab).getView();
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    @SmallTest
    public void testContentChange() {
        View view = mock(View.class);
        View anotherView = mock(View.class);
        InOrder inOrder = Mockito.inOrder(mScrollCaptureManagerDelegateMock);

        // No view available
        mScrollCaptureManager.onContentChanged(mTab);

        // View is set
        when(mTab.getView()).thenReturn(view);
        mScrollCaptureManager.onContentChanged(mTab);
        inOrder.verify(mScrollCaptureManagerDelegateMock).addScrollCaptureBindings(eq(view));

        // Content change
        when(mTab.getView()).thenReturn(anotherView);
        mScrollCaptureManager.onContentChanged(mTab);
        inOrder.verify(mScrollCaptureManagerDelegateMock).removeScrollCaptureBindings(eq(view));
        inOrder.verify(mScrollCaptureManagerDelegateMock).addScrollCaptureBindings(eq(anotherView));

        // Test when native page
        when(mTab.isNativePage()).thenReturn(true);
        mScrollCaptureManager.onContentChanged(mTab);
        inOrder.verify(mScrollCaptureManagerDelegateMock)
                .removeScrollCaptureBindings(eq(anotherView));
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    @SmallTest
    public void testDestroy() {
        View view = mock(View.class);
        InOrder inOrder = Mockito.inOrder(mTab, mScrollCaptureManagerDelegateMock);

        when(mTab.getView()).thenReturn(view);
        mTabSupplier.set(mTab);
        mScrollCaptureManager.destroy();
        inOrder.verify(mTab).removeObserver(mScrollCaptureManager);
        inOrder.verify(mScrollCaptureManagerDelegateMock).removeScrollCaptureBindings(eq(view));
    }
}
