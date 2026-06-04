// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.fragment.app.FragmentActivity;
import androidx.pdf.viewer.fragment.PdfViewerFragment;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.native_page.NativePage;

import java.util.ArrayList;

/** Unit tests for {@link PdfFragmentViewTrackerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PdfFragmentViewTrackerImplUnitTest {
    private static final int TAB_ID1 = 123;
    private static final int TAB_ID2 = 124;
    private static final int TAB_ID3 = 125;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private View mPdfViewerFragmentView1;
    @Mock private View mPdfViewerFragmentView2;
    @Mock private View mPdfViewerFragmentView3;

    private PdfFragmentViewTrackerImpl mPdfFragmentViewTracker;

    @Before
    public void setUp() {
        String tabId1 = String.valueOf(TAB_ID1);
        String tabId2 = String.valueOf(TAB_ID2);
        String tabId3 = String.valueOf(TAB_ID3);
        var fragment = new PdfViewerFragment();
        var fragmentTagKey = R.id.fragment_container_view_tag;
        var lp =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);

        when(mPdfViewerFragmentView1.getTag()).thenReturn(tabId1);
        when(mPdfViewerFragmentView2.getTag()).thenReturn(tabId2);
        when(mPdfViewerFragmentView3.getTag()).thenReturn(tabId3);
        when(mPdfViewerFragmentView1.getTag(eq(fragmentTagKey))).thenReturn(fragment);
        when(mPdfViewerFragmentView2.getTag(eq(fragmentTagKey))).thenReturn(fragment);
        when(mPdfViewerFragmentView3.getTag(eq(fragmentTagKey))).thenReturn(fragment);
        when(mPdfViewerFragmentView1.getLayoutParams()).thenReturn(lp);
        when(mPdfViewerFragmentView2.getLayoutParams()).thenReturn(lp);
        when(mPdfViewerFragmentView3.getLayoutParams()).thenReturn(lp);

        // Starts with all the views in |mPdfFragmentViews|.
        var pdfFragmentViews = new ArrayList<View>();
        pdfFragmentViews.add(mPdfViewerFragmentView1);
        pdfFragmentViews.add(mPdfViewerFragmentView2);
        pdfFragmentViews.add(mPdfViewerFragmentView3);

        mPdfFragmentViewTracker =
                new PdfFragmentViewTrackerImpl(
                        mTabModelSelector, Mockito.mock(FragmentActivity.class));
        mPdfFragmentViewTracker.setFragmentSupplierForTesting(() -> pdfFragmentViews);
    }

    @Test
    public void test_maybeRelocatedViews_removeMismatchedView() {
        ViewGroup container = Mockito.mock(ViewGroup.class);
        when(container.getChildCount()).thenReturn(2);
        when(container.getChildAt(eq(0))).thenReturn(mPdfViewerFragmentView1);
        when(container.getChildAt(eq(1))).thenReturn(mPdfViewerFragmentView2);
        assertEquals(3, mPdfFragmentViewTracker.getViewsForTesting().size());

        String tabId = String.valueOf(TAB_ID1);
        mPdfFragmentViewTracker.maybeRelocateViews(container, tabId);

        verify(container).removeView(eq(mPdfViewerFragmentView2));
        assertEquals(2, mPdfFragmentViewTracker.getViewsForTesting().size());
    }

    @Test
    public void test_maybeRelocatedViews_placeMatchedView() {
        ViewGroup container = Mockito.mock(ViewGroup.class);
        when(container.getChildCount()).thenReturn(0);
        assertEquals(3, mPdfFragmentViewTracker.getViewsForTesting().size());

        String tabId = String.valueOf(TAB_ID1);
        mPdfFragmentViewTracker.maybeRelocateViews(container, tabId);

        verify(container).addView(eq(mPdfViewerFragmentView1));
        assertEquals(2, mPdfFragmentViewTracker.getViewsForTesting().size());
        assertFalse(mPdfFragmentViewTracker.getViewsForTesting().contains(mPdfViewerFragmentView1));
    }

    @Test
    public void testDestroy_removeViews() {
        Tab tab = Mockito.mock(Tab.class);
        NativePage pdfPage = Mockito.mock(NativePage.class);
        when(tab.getId()).thenReturn(TAB_ID3);
        when(tab.getNativePage()).thenReturn(pdfPage);
        when(pdfPage.isPdf()).thenReturn(true);

        assertTrue(mPdfFragmentViewTracker.getViewsForTesting().contains(mPdfViewerFragmentView3));

        // Verifies that destroying a PDF Tab removes the matching PdfViewerFragment View
        // from the tracker.
        mPdfFragmentViewTracker.destroyPdfTabForTesting(tab);
        assertFalse(mPdfFragmentViewTracker.getViewsForTesting().contains(mPdfViewerFragmentView3));
    }
}
