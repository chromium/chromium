// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.tab_management.CustomMessageCardViewProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.CustomMessageCardViewProperties.MESSAGE_CARD_VIEW;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import org.junit.Assert;
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

        mCustomMessageCardView =
                (CustomMessageCardView)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.custom_message_card_item, /* root= */ null);

        mModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(
                                MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE,
                                MessageCardViewProperties.MessageCardScope.BOTH)
                        .with(CARD_TYPE, TabListModel.CardProperties.ModelType.MESSAGE)
                        .build();

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel,
                        new CustomMessageCardViewBinder.ViewHolder(
                                mCustomMessageCardView, mProvider),
                        CustomMessageCardViewBinder::bind);
    }

    @Test
    public void testSetChildView() {
        mModel.set(MESSAGE_CARD_VIEW, mChildView);
        Assert.assertEquals(1, mCustomMessageCardView.getChildCount());
    }

    @Test
    public void testSetCardAlpha() {
        mModel.set(CARD_ALPHA, 1f);
        Assert.assertEquals(1f, mCustomMessageCardView.getAlpha(), MathUtils.EPSILON);
    }

    @Test
    public void testSetIsIncognito() {
        mModel.set(IS_INCOGNITO, true);
        verify(mProvider, times(1)).setIsIncognito(true);
    }
}
