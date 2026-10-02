// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
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
import org.robolectric.Robolectric;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
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

    private View mPdfViewerFragmentView1;
    private View mPdfViewerFragmentView2;
    private View mPdfViewerFragmentView3;
    private PdfFragmentViewTrackerImpl mPdfFragmentViewTracker;

    @Before
    public void setUp() {
        when(mTabModelSelector.getCurrentTabModelSupplier())
                .thenReturn(ObservableSuppliers.createMonotonic());
        String tabId1 = String.valueOf(TAB_ID1);
        String tabId2 = String.valueOf(TAB_ID2);
        String tabId3 = String.valueOf(TAB_ID3);
        var fragment = new PdfViewerFragment();
        var fragmentTagKey = R.id.fragment_container_view_tag;

        var context = ContextUtils.getApplicationContext();
        mPdfViewerFragmentView1 = new View(context);
        mPdfViewerFragmentView2 = new View(context);
        mPdfViewerFragmentView3 = new View(context);
        mPdfViewerFragmentView1.setTag(tabId1);
        mPdfViewerFragmentView2.setTag(tabId2);
        mPdfViewerFragmentView3.setTag(tabId3);
        mPdfViewerFragmentView1.setTag(fragmentTagKey, fragment);
        mPdfViewerFragmentView2.setTag(fragmentTagKey, fragment);
        mPdfViewerFragmentView3.setTag(fragmentTagKey, fragment);

        // Starts with all the views in |mPdfFragmentViews|.
        var pdfFragmentViews = new ArrayList<View>();
        pdfFragmentViews.add(mPdfViewerFragmentView1);
        pdfFragmentViews.add(mPdfViewerFragmentView2);
        pdfFragmentViews.add(mPdfViewerFragmentView3);

        mPdfFragmentViewTracker =
                new PdfFragmentViewTrackerImpl(
                        mTabModelSelector, Robolectric.buildActivity(FragmentActivity.class).get());
        mPdfFragmentViewTracker.setFragmentSupplierForTesting(() -> pdfFragmentViews);
    }

    @Test
    public void test_maybeRelocatedViews_removeMismatchedView() {
        ViewGroup container = new FrameLayout(ContextUtils.getApplicationContext());
        container.addView(mPdfViewerFragmentView1);
        container.addView(mPdfViewerFragmentView2);
        assertEquals(3, mPdfFragmentViewTracker.getViewsForTesting().size());

        String tabId = String.valueOf(TAB_ID1);
        mPdfFragmentViewTracker.maybeRelocateViews(container, tabId);

        assertEquals(1, container.getChildCount());
        assertEquals(mPdfViewerFragmentView1, container.getChildAt(0));
        assertEquals(2, mPdfFragmentViewTracker.getViewsForTesting().size());
    }

    @Test
    public void test_maybeRelocatedViews_placeMatchedView() {
        ViewGroup container = new FrameLayout(ContextUtils.getApplicationContext());
        assertEquals(3, mPdfFragmentViewTracker.getViewsForTesting().size());

        String tabId = String.valueOf(TAB_ID1);
        mPdfFragmentViewTracker.maybeRelocateViews(container, tabId);

        assertEquals(1, container.getChildCount());
        assertEquals(mPdfViewerFragmentView1, container.getChildAt(0));
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
