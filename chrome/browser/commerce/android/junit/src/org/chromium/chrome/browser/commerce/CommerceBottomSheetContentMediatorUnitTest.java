// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
public class CommerceBottomSheetContentMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock View mContentItemCustomView;
    @Mock BottomSheetController mBottomSheetController;
    @Mock View mContentView;
    private ModelList mModelList;
    private CommerceBottomSheetContentMediator mMediator;

    @Before
    public void setup() {
        mModelList = new ModelList();
    }

    private void setupMediator(int expectedContentCount) {
        mMediator =
                new CommerceBottomSheetContentMediator(
                        mModelList, expectedContentCount, mBottomSheetController, mContentView);
    }

    private PropertyModel createPropertyModel(int type) {
        return new PropertyModel.Builder(CommerceBottomSheetContentProperties.ALL_KEYS)
                .with(CommerceBottomSheetContentProperties.TYPE, type)
                .with(CommerceBottomSheetContentProperties.HAS_TITLE, true)
                .with(CommerceBottomSheetContentProperties.TITLE, "title")
                .with(CommerceBottomSheetContentProperties.CUSTOM_VIEW, mContentItemCustomView)
                .build();
    }

    @Test(expected = AssertionError.class)
    public void testOnContentReady_assertOnInvalidPropertyModel() {
        setupMediator(/* expectedContentCount= */ 1);
        mMediator.onContentReady(new PropertyModel());
    }

    @Test
    public void testOnContentReady_firstPropertyModel() {
        setupMediator(/* expectedContentCount= */ 1);
        mMediator.onContentReady(createPropertyModel(0));

        assertEquals(1, mModelList.size());
        verify(mBottomSheetController, times(1)).requestShowContent(any(), eq(true));
    }

    @Test
    public void testOnContentReady_MultiPropertyModels() {
        setupMediator(/* expectedContentCount= */ 3);
        PropertyModel model0 = createPropertyModel(0);
        PropertyModel model1 = createPropertyModel(1);
        PropertyModel model2 = createPropertyModel(2);

        mMediator.onContentReady(model1);
        mMediator.onContentReady(model0);
        mMediator.onContentReady(model2);

        assertEquals(3, mModelList.size());
        assertEquals(model0, mModelList.get(0).model);
        assertEquals(model1, mModelList.get(1).model);
        assertEquals(model2, mModelList.get(2).model);
        verify(mBottomSheetController, times(1)).requestShowContent(any(), eq(true));
    }

    @Test
    public void testOnContentReady_MultiPropertyModels_withAtLeastOneNullPropertyModel() {
        setupMediator(/* expectedContentCount= */ 3);
        PropertyModel model0 = createPropertyModel(0);
        PropertyModel model1 = createPropertyModel(1);

        mMediator.onContentReady(model1);
        mMediator.onContentReady(model0);
        mMediator.onContentReady(null);

        assertEquals(2, mModelList.size());
        assertEquals(model0, mModelList.get(0).model);
        assertEquals(model1, mModelList.get(1).model);
        verify(mBottomSheetController, times(1)).requestShowContent(any(), eq(true));
    }

    @Test
    public void testOnContentReady_MultiPropertyModels_waitingForMoreContent() {
        setupMediator(/* expectedContentCount= */ 3);
        PropertyModel model0 = createPropertyModel(0);
        PropertyModel model1 = createPropertyModel(1);

        mMediator.onContentReady(model1);
        mMediator.onContentReady(model0);

        assertEquals(2, mModelList.size());
        assertEquals(model0, mModelList.get(0).model);
        assertEquals(model1, mModelList.get(1).model);
        verify(mBottomSheetController, never()).requestShowContent(any(), eq(true));
    }

    @Test(expected = AssertionError.class)
    public void testOnContentReady_assertOnSameType() {
        setupMediator(/* expectedContentCount= */ 1);
        PropertyModel model0 = createPropertyModel(0);
        PropertyModel model1 = createPropertyModel(0);

        mMediator.onContentReady(model0);
        mMediator.onContentReady(model1);
    }

    @Test
    public void testOnBottomSheetClosed() {
        setupMediator(/* expectedContentCount= */ 2);
        PropertyModel model0 = createPropertyModel(0);
        PropertyModel model1 = createPropertyModel(1);
        mMediator.onContentReady(model1);
        mMediator.onContentReady(model0);
        assertEquals(2, mModelList.size());

        mMediator.onBottomSheetClosed();
        assertEquals(0, mModelList.size());
    }
}
