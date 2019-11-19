// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.omnibox.status.StatusView.StatusViewDelegate;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.chrome.browser.ui.widget.CompositeTouchDelegate;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Render tests for {@link StatusView}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class StatusViewRenderTest extends DummyUiActivityTestCase {
    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("chrome/test/data/android/render_tests");

    private StatusView mStatusView;
    private PropertyModel mStatusModel;

    /** Testing implementation that returns true for everything. */
    static class DelegateForTesting extends StatusViewDelegate {
        @Override
        boolean shouldShowSearchEngineLogo(boolean isIncognito) {
            return true;
        }

        @Override
        boolean isSearchEngineLogoEnabled() {
            return true;
        }
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        runOnUiThreadBlocking(() -> {
            ViewGroup view = new LinearLayout(getActivity());

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);

            getActivity().setContentView(view, params);

            mStatusView = getActivity()
                                  .getLayoutInflater()
                                  .inflate(org.chromium.chrome.R.layout.location_status, view, true)
                                  .findViewById(org.chromium.chrome.R.id.location_bar_status);
            mStatusView.setCompositeTouchDelegate(new CompositeTouchDelegate(view));
            mStatusView.setToolbarCommonPropertiesModel(
                    new LocationBarModel(mStatusView.getContext()));
            mStatusModel = new PropertyModel.Builder(StatusProperties.ALL_KEYS).build();
            PropertyModelChangeProcessor.create(mStatusModel, mStatusView, new StatusViewBinder());
        });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStatusViewVerbosePadding() throws Exception {
        runOnUiThreadBlocking(() -> {
            mStatusView.setVerboseStatusTextContent(
                    org.chromium.chrome.R.string.location_bar_preview_lite_page_status);
            mStatusView.setVerboseStatusTextWidth(mStatusView.getResources().getDimensionPixelSize(
                    org.chromium.chrome.R.dimen.location_bar_min_verbose_status_text_width));
            mStatusView.setVerboseStatusTextVisible(true);
            mStatusModel.set(
                    StatusProperties.STATUS_ICON_RES, org.chromium.chrome.R.drawable.ic_search);
            mStatusModel.set(StatusProperties.STATUS_ICON_TINT_RES, 0);
        });
        mRenderTestRule.render(mStatusView, "status_view_verbose_padding");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStatusViewVerbosePadding_withDSEIcon() throws Exception {
        runOnUiThreadBlocking(() -> {
            mStatusView.setDelegateForTesting(new DelegateForTesting());

            mStatusView.updateSearchEngineStatusIcon(true, true, "");
            mStatusView.setVerboseStatusTextContent(
                    org.chromium.chrome.R.string.location_bar_preview_lite_page_status);
            mStatusView.setVerboseStatusTextWidth(mStatusView.getResources().getDimensionPixelSize(
                    org.chromium.chrome.R.dimen.location_bar_min_verbose_status_text_width));
            mStatusView.setVerboseStatusTextVisible(true);
            mStatusModel.set(StatusProperties.STATUS_ALPHA, 1f);
            mStatusModel.set(StatusProperties.SHOW_STATUS_ICON, true);
            mStatusModel.set(StatusProperties.STATUS_ICON_RES,
                    org.chromium.chrome.R.drawable.ic_logo_googleg_24dp);
            mStatusModel.set(StatusProperties.STATUS_ICON_TINT_RES, 0);
        });

        mRenderTestRule.render(mStatusView, "status_view_verbose_padding_with_dse_icon");
    }
}