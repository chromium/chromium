// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DESCRIPTION_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.DismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ReviewActionProvider;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Unit tests for {@link CollaborationActivityMessageCardViewModelUnitTest}. Note there is no
 * exhaustive test of the property model as that would simply be a change detection test which isn't
 * worthwhile here.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class CollaborationActivityMessageCardViewModelUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ReviewActionProvider mActionHandler;
    @Mock private DismissActionProvider mDismissHandler;

    private Context mContext;
    private CollaborationActivityMessageCardViewModel mModel;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mModel =
                new CollaborationActivityMessageCardViewModel(
                        mContext, mActionHandler, mDismissHandler);
    }

    @Test
    public void testActionHandlers() {
        PropertyModel model = mModel.getPropertyModel();

        model.get(MESSAGE_SERVICE_ACTION_PROVIDER).review();
        verify(mActionHandler).review();

        int messageType = 3423;
        model.get(MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER).dismiss(messageType);
        verify(mDismissHandler).dismiss(messageType);
    }

    @Test
    public void testUpdateTextDescription() {
        PropertyModel model = mModel.getPropertyModel();

        mModel.updateDescriptionText(
                mContext, /* tabsAdded= */ 0, /* tabsChanged= */ 0, /* tabsClosed= */ 0);
        assertEquals("No tab updates", model.get(DESCRIPTION_TEXT));

        mModel.updateDescriptionText(
                mContext, /* tabsAdded= */ 1, /* tabsChanged= */ 0, /* tabsClosed= */ 0);
        assertEquals("1 tab added", model.get(DESCRIPTION_TEXT));
        mModel.updateDescriptionText(
                mContext, /* tabsAdded= */ 2, /* tabsChanged= */ 0, /* tabsClosed= */ 0);
        assertEquals("2 tabs added", model.get(DESCRIPTION_TEXT));

        mModel.updateDescriptionText(
                mContext, /* tabsAdded= */ 0, /* tabsChanged= */ 1, /* tabsClosed= */ 0);
        assertEquals("1 tab changed", model.get(DESCRIPTION_TEXT));
        mModel.updateDescriptionText(
                mContext, /* tabsAdded= */ 0, /* tabsChanged= */ 2, /* tabsClosed= */ 0);
        assertEquals("2 tabs changed", model.get(DESCRIPTION_TEXT));

        mModel.updateDescriptionText(
                mContext, /* tabsAdded= */ 0, /* tabsChanged= */ 0, /* tabsClosed= */ 1);
        assertEquals("1 tab closed", model.get(DESCRIPTION_TEXT));
        mModel.updateDescriptionText(
                mContext, /* tabsAdded= */ 0, /* tabsChanged= */ 0, /* tabsClosed= */ 2);
        assertEquals("2 tabs closed", model.get(DESCRIPTION_TEXT));

        mModel.updateDescriptionText(
                mContext, /* tabsAdded= */ 1, /* tabsChanged= */ 2, /* tabsClosed= */ 0);
        assertEquals("1 tab added, 2 changed", model.get(DESCRIPTION_TEXT));
        mModel.updateDescriptionText(
                mContext, /* tabsAdded= */ 2, /* tabsChanged= */ 3, /* tabsClosed= */ 0);
        assertEquals("2 tabs added, 3 changed", model.get(DESCRIPTION_TEXT));

        mModel.updateDescriptionText(
                mContext, /* tabsAdded= */ 1, /* tabsChanged= */ 0, /* tabsClosed= */ 2);
        assertEquals("1 tab added, 2 closed", model.get(DESCRIPTION_TEXT));
        mModel.updateDescriptionText(
                mContext, /* tabsAdded= */ 2, /* tabsChanged= */ 0, /* tabsClosed= */ 3);
        assertEquals("2 tabs added, 3 closed", model.get(DESCRIPTION_TEXT));

        mModel.updateDescriptionText(
                mContext, /* tabsAdded= */ 0, /* tabsChanged= */ 1, /* tabsClosed= */ 2);
        assertEquals("1 tab changed, 2 closed", model.get(DESCRIPTION_TEXT));
        mModel.updateDescriptionText(
                mContext, /* tabsAdded= */ 0, /* tabsChanged= */ 2, /* tabsClosed= */ 3);
        assertEquals("2 tabs changed, 3 closed", model.get(DESCRIPTION_TEXT));

        mModel.updateDescriptionText(
                mContext, /* tabsAdded= */ 1, /* tabsChanged= */ 2, /* tabsClosed= */ 3);
        assertEquals("1 tab added, 2 changed, 3 closed", model.get(DESCRIPTION_TEXT));
        mModel.updateDescriptionText(
                mContext, /* tabsAdded= */ 2, /* tabsChanged= */ 3, /* tabsClosed= */ 4);
        assertEquals("2 tabs added, 3 changed, 4 closed", model.get(DESCRIPTION_TEXT));
    }
}
