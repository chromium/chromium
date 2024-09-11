// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import static org.junit.Assert.assertEquals;

import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
public class CommerceBottomSheetContentMediatorUnitTest {
    @Mock View mContentCustomView;
    private ModelList mModelList;
    private CommerceBottomSheetContentMediator mMediator;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mModelList = new ModelList();
        mMediator = new CommerceBottomSheetContentMediator(mModelList);
    }

    private PropertyModel createPropertyModel(int type) {
        return new PropertyModel.Builder(CommerceBottomSheetContentProperties.ALL_KEYS)
                .with(CommerceBottomSheetContentProperties.TYPE, type)
                .with(CommerceBottomSheetContentProperties.HAS_TITLE, true)
                .with(CommerceBottomSheetContentProperties.TITLE, "title")
                .with(CommerceBottomSheetContentProperties.CUSTOM_VIEW, mContentCustomView)
                .build();
    }

    @Test(expected = AssertionError.class)
    public void testOnContentReady_assertOnInvalidPropertyModel() {
        mMediator.onContentReady(new PropertyModel());
    }

    @Test
    public void testOnContentReady_firstPropertyModel() {
        mMediator.onContentReady(createPropertyModel(0));

        assertEquals(1, mModelList.size());
    }

    @Test
    public void testOnContentReady_MultiPropertyModels() {
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
    }

    @Test(expected = AssertionError.class)
    public void testOnContentReady_assertOnSameType() {
        PropertyModel model0 = createPropertyModel(0);
        PropertyModel model1 = createPropertyModel(0);

        mMediator.onContentReady(model0);
        mMediator.onContentReady(model1);
    }
}
