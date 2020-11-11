// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.TrendyTermsCoordinator.ItemType.TRENDY_TERMS;
import static org.chromium.chrome.browser.tasks.TrendyTermsProperties.TRENDY_TERM_ICON_DRAWABLE_ID;
import static org.chromium.chrome.browser.tasks.TrendyTermsProperties.TRENDY_TERM_STRING;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.io.IOException;

/** Tests for {@link TrendyTermsCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TrendyTermsCoordinatorTest extends DummyUiActivityTestCase {
    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    private TrendyTermsCoordinator mCoordinator;
    private RecyclerView mRecyclerView;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRecyclerView = new RecyclerView(getActivity());
            getActivity().setContentView(mRecyclerView);
            mRecyclerView.setVisibility(View.VISIBLE);
            mCoordinator = new TrendyTermsCoordinator(
                    getActivity(), mRecyclerView, new ObservableSupplierImpl<>());
        });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderTrendyTerms() throws IOException {
        String[] trendyTerms = {"COVID-19 symptoms", "Trendy Terms", "Sand Wedge"};
        for (String trendyTerm : trendyTerms) {
            PropertyModel trendInfo =
                    new PropertyModel.Builder(TrendyTermsProperties.ALL_KEYS)
                            .with(TRENDY_TERM_STRING, trendyTerm)
                            .with(TRENDY_TERM_ICON_DRAWABLE_ID, R.drawable.ic_search)
                            .build();
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> mCoordinator.addTrendyTermForTesting(TRENDY_TERMS, trendInfo));
        }
        mRenderTestRule.render(mRecyclerView, "start_surface_trendy_terms");
    }
}
