// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;

import static org.chromium.ui.test.util.RenderTestRule.Component.UI_BROWSER_SHOPPING;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Render Tests for the View build by the CommerceBottomSheetContent component. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class CommerceBottomSheetContentRenderTest {
    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus().setBugComponent(UI_BROWSER_SHOPPING).build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock BottomSheetController mBottomSheetController;

    private ModelList mModelList;
    private View mContentView;
    private RecyclerView mRecyclerView;
    private CommerceBottomSheetContentCoordinator mCoordinator;

    private PropertyModel createPropertyModel(int type, boolean hasTitle) {
        TextView view = new TextView(getActivity());
        view.setText("custom view");
        return new PropertyModel.Builder(CommerceBottomSheetContentProperties.ALL_KEYS)
                .with(CommerceBottomSheetContentProperties.TYPE, type)
                .with(CommerceBottomSheetContentProperties.HAS_TITLE, hasTitle)
                .with(CommerceBottomSheetContentProperties.TITLE, "Title " + type)
                .with(CommerceBottomSheetContentProperties.CUSTOM_VIEW, view)
                .build();
    }

    private BlankUiTestActivity getActivity() {
        return mActivityTestRule.getActivity();
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);

        doNothing().when(mBottomSheetController).addObserver(any());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup rootView = new FrameLayout(getActivity());
                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT);
                    getActivity().setContentView(rootView, params);

                    mCoordinator =
                            new CommerceBottomSheetContentCoordinator(
                                    getActivity(), mBottomSheetController);

                    mContentView = mCoordinator.getContentViewForTesting();
                    mRecyclerView = mCoordinator.getRecyclerViewForTesting();
                    mModelList = mCoordinator.getModelListForTesting();
                    rootView.addView(mContentView);
                });
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testSingleContent() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModelList.add(new ListItem(0, createPropertyModel(0, true)));
                });
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mRecyclerView);
        mRenderTestRule.render(mContentView, "single_content");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testMultipleContents() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModelList.add(new ListItem(0, createPropertyModel(0, true)));
                    mModelList.add(new ListItem(0, createPropertyModel(1, false)));
                    mModelList.add(new ListItem(0, createPropertyModel(2, false)));
                });
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mRecyclerView);
        mRenderTestRule.render(mContentView, "multiple_content");
    }
}
