// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.scroll_capture;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.when;

import android.os.Build.VERSION_CODES;
import android.view.View;

import androidx.annotation.RequiresApi;
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

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Tests for the ScreenshotBoundsManager */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@RequiresApi(api = VERSION_CODES.S)
@MinAndroidSdkLevel(VERSION_CODES.S)
public class ScrollCaptureManagerTest {
    @Mock
    private Tab mTab;

    private ObservableSupplierImpl<Tab> mTabSupplier;
    // We should use the Object type here to avoid RuntimeError in classloader on the bots running
    // API versions before S.
    private Object mScrollCaptureManagerObj;
    private Object mScrollCaptureCallbackMockObj;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabSupplier = new ObservableSupplierImpl<>();
            mScrollCaptureCallbackMockObj = mock(ScrollCaptureCallbackImpl.class);
            mScrollCaptureManagerObj = new ScrollCaptureManager(
                    mTabSupplier, (ScrollCaptureCallbackImpl) mScrollCaptureCallbackMockObj);
        });
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testObserveTab() {
        ScrollCaptureManager scrollCaptureManager = (ScrollCaptureManager) mScrollCaptureManagerObj;
        ScrollCaptureCallbackImpl scrollCaptureCallbackMock =
                (ScrollCaptureCallbackImpl) mScrollCaptureCallbackMockObj;
        Tab tab = mock(Tab.class);
        InOrder inOrder = Mockito.inOrder(mTab, tab, scrollCaptureCallbackMock);

        mTabSupplier.set(mTab);
        inOrder.verify(scrollCaptureCallbackMock).setCurrentTab(mTab);
        inOrder.verify(mTab).addObserver(scrollCaptureManager);
        inOrder.verify(mTab).getView();

        mTabSupplier.set(tab);
        inOrder.verify(mTab).removeObserver(scrollCaptureManager);
        inOrder.verify(scrollCaptureCallbackMock).setCurrentTab(tab);
        inOrder.verify(tab).addObserver(scrollCaptureManager);
        inOrder.verify(tab).getView();
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testContentChange() {
        ScrollCaptureManager scrollCaptureManager = (ScrollCaptureManager) mScrollCaptureManagerObj;
        View view = mock(View.class);
        View anotherView = mock(View.class);
        InOrder inOrder = Mockito.inOrder(view, anotherView);

        // No view available
        scrollCaptureManager.onContentChanged(mTab);
        inOrder.verify(view, times(0)).setScrollCaptureHint(anyInt());

        // View is set
        when(mTab.getView()).thenReturn(view);
        scrollCaptureManager.onContentChanged(mTab);
        inOrder.verify(view).setScrollCaptureHint(eq(View.SCROLL_CAPTURE_HINT_INCLUDE));

        // Content change
        when(mTab.getView()).thenReturn(anotherView);
        scrollCaptureManager.onContentChanged(mTab);
        inOrder.verify(view).setScrollCaptureHint(View.SCROLL_CAPTURE_HINT_AUTO);
        inOrder.verify(anotherView).setScrollCaptureHint(View.SCROLL_CAPTURE_HINT_INCLUDE);

        // Test when native page
        when(mTab.isNativePage()).thenReturn(true);
        scrollCaptureManager.onContentChanged(mTab);
        inOrder.verify(anotherView).setScrollCaptureHint(View.SCROLL_CAPTURE_HINT_AUTO);
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testDestroy() {
        ScrollCaptureManager scrollCaptureManager = (ScrollCaptureManager) mScrollCaptureManagerObj;
        View view = mock(View.class);
        InOrder inOrder = Mockito.inOrder(mTab, view);

        when(mTab.getView()).thenReturn(view);
        mTabSupplier.set(mTab);
        scrollCaptureManager.destroy();
        inOrder.verify(mTab).removeObserver(scrollCaptureManager);
        inOrder.verify(view).setScrollCaptureHint(View.SCROLL_CAPTURE_HINT_AUTO);
    }
}
