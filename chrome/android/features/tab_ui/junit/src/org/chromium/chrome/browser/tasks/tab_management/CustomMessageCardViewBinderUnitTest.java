// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MessageCardScope;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Robolectric tests for {@link CustomMessageCardViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomMessageCardViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private CustomMessageCardProvider mProvider;

    private View mChildView;
    private Activity mActivity;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private CustomMessageCardView mCustomMessageCardView;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;

        mChildView = new FrameLayout(mActivity);

        doReturn(mChildView).when(mProvider).getCustomView();
        doReturn(MessageCardViewProperties.MessageCardScope.BOTH)
                .when(mProvider)
                .getMessageCardVisibilityControl();
        doReturn(ModelType.MESSAGE).when(mProvider).getCardType();
        doReturn(MessageType.ARCHIVED_TABS_MESSAGE).when(mProvider).getMessageType();

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
        assertEquals(
                MessageCardScope.REGULAR,
                mModel.get(MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE));
        assertEquals(1f, mModel.get(CARD_ALPHA), MathUtils.EPSILON);
        assertEquals(CardProperties.ModelType.MESSAGE, mModel.get(CARD_TYPE));
        assertEquals(MessageType.ARCHIVED_TABS_MESSAGE, mModel.get(MESSAGE_TYPE));

        mModel.set(MessageCardViewProperties.IS_INCOGNITO, true);
        verify(mProvider, times(1)).setIsIncognito(true);
    }

    @Test
    public void testRebindView() {
        assertEquals(mCustomMessageCardView, mChildView.getParent());

        mPropertyModelChangeProcessor.destroy();
        // Rebind while reusing the prior view. This should not throw an exception since the parent
        // view will be removed before reattaching.
        PropertyModelChangeProcessor.create(
                mModel, mCustomMessageCardView, CustomMessageCardViewBinder::bind);

        assertEquals(mCustomMessageCardView, mChildView.getParent());
    }
}
