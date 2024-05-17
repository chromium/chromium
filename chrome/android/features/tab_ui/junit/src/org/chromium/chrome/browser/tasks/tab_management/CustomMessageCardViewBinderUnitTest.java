// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Robolectric tests for {@link CustomMessageCardViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomMessageCardViewBinderUnitTest {
    @Mock private CustomMessageCardProvider mProvider;
    @Mock private View mChildView;

    private Activity mActivity;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private CustomMessageCardView mCustomMessageCardView;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();

        doReturn(mChildView).when(mProvider).getCustomView();
        doReturn(MessageCardViewProperties.MessageCardScope.BOTH)
                .when(mProvider)
                .getMessageCardVisibilityControl();
        doReturn(TabListModel.CardProperties.ModelType.MESSAGE).when(mProvider).getCardType();

        mCustomMessageCardView =
                (CustomMessageCardView)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.custom_message_card_item, /* root= */ null);

        mModel = CustomMessageCardViewModel.create(mProvider);

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mCustomMessageCardView, CustomMessageCardViewBinder::bind);
    }

    @Test
    public void testSetup() {
        assertEquals(1, mCustomMessageCardView.getChildCount());
        assertEquals(1f, mCustomMessageCardView.getAlpha(), MathUtils.EPSILON);

        mModel.set(MessageCardViewProperties.IS_INCOGNITO, true);
        verify(mProvider, times(1)).setIsIncognito(true);
    }
}
