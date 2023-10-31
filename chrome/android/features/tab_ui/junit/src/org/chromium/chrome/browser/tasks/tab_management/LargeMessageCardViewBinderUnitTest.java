// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;

import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link LargeMessageCardViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LargeMessageCardViewBinderUnitTest {
    private static final String FAKE_DISPLAY_TEXT = "Fake Text";
    private static final int FAKE_ICON_WIDTH = 666;
    private static final int FAKE_ICON_HEIGHT = 999;

    @Mock LargeMessageCardView mMockLargeCardView;

    @Mock MessageCardView.DismissActionProvider mMockDismissActionProvider1;

    @Mock MessageCardView.DismissActionProvider mMockDismissActionProvider2;

    @Mock MessageCardView.ReviewActionProvider mMockReviewActionProvider1;

    @Mock MessageCardView.ReviewActionProvider mMockReviewActionProvider2;

    @Mock ShoppingPersistedTabData.PriceDrop mMockPriceDrop;

    @Mock MessageCardView.IconProvider mMockIconProvider;

    @Mock Drawable mMockDrawable;

    @Mock private OnClickListener mMockOnClickListenerMock;

    @Captor ArgumentCaptor<Callback<Drawable>> mCallbackDrawableArgumentCaptor;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mModel = new PropertyModel(MessageCardViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mModel, mMockLargeCardView, LargeMessageCardViewBinder::bind);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(
                mMockLargeCardView,
                mMockDismissActionProvider1,
                mMockDismissActionProvider2,
                mMockReviewActionProvider1,
                mMockReviewActionProvider2,
                mMockPriceDrop,
                mMockIconProvider,
                mMockDrawable,
                mMockOnClickListenerMock);
    }

    @Test
    @SmallTest
    public void updateContntDescriptionText() {
        mModel.set(MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION, FAKE_DISPLAY_TEXT);
        verify(mMockLargeCardView, times(1)).setDismissButtonContentDescription(FAKE_DISPLAY_TEXT);
        verify(mMockLargeCardView, times(1)).setDismissButtonOnClickListener(any());

        mModel.set(MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION, null);
        verify(mMockLargeCardView, times(1)).setDismissButtonContentDescription(null);
        verify(mMockLargeCardView, times(2)).setDismissButtonOnClickListener(any());
    }

    @Test
    @SmallTest
    public void updateActionButtonText() {
        mModel.set(MessageCardViewProperties.ACTION_TEXT, FAKE_DISPLAY_TEXT);
        verify(mMockLargeCardView, times(1)).setActionText(FAKE_DISPLAY_TEXT);
        verify(mMockLargeCardView, times(1)).setActionButtonOnClickListener(any());

        mModel.set(MessageCardViewProperties.ACTION_TEXT, null);
        verify(mMockLargeCardView, times(1)).setActionText(null);
        verify(mMockLargeCardView, times(2)).setActionButtonOnClickListener(any());
    }

    @Test
    @SmallTest
    public void updateSecondaryActionButtonText() {
        mModel.set(MessageCardViewProperties.SECONDARY_ACTION_TEXT, FAKE_DISPLAY_TEXT);
        verify(mMockLargeCardView, times(1)).setSecondaryActionText(FAKE_DISPLAY_TEXT);
    }

    @Test
    @SmallTest
    public void updateSecondaryActionButtonOnClickListener() {
        mModel.set(
                MessageCardViewProperties.SECONDARY_ACTION_BUTTON_CLICK_HANDLER,
                mMockOnClickListenerMock);
        verify(mMockLargeCardView, times(1))
                .setSecondaryActionButtonOnClickListener(mMockOnClickListenerMock);
    }

    @Test
    @SmallTest
    public void updateTitleText() {
        mModel.set(MessageCardViewProperties.TITLE_TEXT, FAKE_DISPLAY_TEXT);
        verify(mMockLargeCardView, times(1)).setTitleText(FAKE_DISPLAY_TEXT);

        mModel.set(MessageCardViewProperties.TITLE_TEXT, null);
        verify(mMockLargeCardView, times(1)).setTitleText(null);
    }

    @Test
    @SmallTest
    public void updatePriceDropInfo() {
        mModel.set(MessageCardViewProperties.PRICE_DROP, mMockPriceDrop);
        verify(mMockLargeCardView, times(1)).setupPriceInfoBox(mMockPriceDrop);
    }

    @Test
    @SmallTest
    public void updateIconVisibility() {
        mModel.set(MessageCardViewProperties.IS_ICON_VISIBLE, true);
        verify(mMockLargeCardView, times(1)).setIconVisibility(true);

        mModel.set(MessageCardViewProperties.IS_ICON_VISIBLE, false);
        verify(mMockLargeCardView, times(1)).setIconVisibility(false);
    }

    @Test
    @SmallTest
    public void updateIconWidth() {
        mModel.set(MessageCardViewProperties.ICON_WIDTH_IN_PIXELS, FAKE_ICON_WIDTH);
        verify(mMockLargeCardView, times(1)).updateIconWidth(FAKE_ICON_WIDTH);
    }

    @Test
    @SmallTest
    public void updateIconHeight() {
        mModel.set(MessageCardViewProperties.ICON_HEIGHT_IN_PIXELS, FAKE_ICON_HEIGHT);
        verify(mMockLargeCardView, times(1)).updateIconHeight(FAKE_ICON_HEIGHT);
    }

    @Test
    @SmallTest
    public void updateCloseIconVisibility() {
        mModel.set(MessageCardViewProperties.IS_CLOSE_BUTTON_VISIBLE, false);
        verify(mMockLargeCardView, times(1)).setCloseButtonVisibility(false);

        mModel.set(MessageCardViewProperties.IS_CLOSE_BUTTON_VISIBLE, true);
        verify(mMockLargeCardView, times(1)).setCloseButtonVisibility(true);
    }

    @Test
    @SmallTest
    public void updateIconDrawable() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(MessageCardViewProperties.ICON_PROVIDER, mMockIconProvider)
                        .build();

        LargeMessageCardViewBinder.updateIconDrawable(mModel, mMockLargeCardView);
        verify(mMockIconProvider).fetchIconDrawable(mCallbackDrawableArgumentCaptor.capture());
        Callback<Drawable> callback = mCallbackDrawableArgumentCaptor.getValue();
        callback.onResult(mMockDrawable);
        verify(mMockLargeCardView, times(1)).setIconDrawable(mMockDrawable);
    }

    @Test
    @SmallTest
    public void updateCardAlpha() {
        mModel.set(CARD_ALPHA, 0.5f);
        verify(mMockLargeCardView, times(1)).setAlpha(0.5f);
    }

    @Test
    @SmallTest
    public void handleDismissActionButton_NoServiceProvider() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(
                                MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER,
                                mMockDismissActionProvider1)
                        .with(
                                MessageCardViewProperties.MESSAGE_TYPE,
                                MessageService.MessageType.FOR_TESTING)
                        .with(
                                MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER,
                                null)
                        .build();

        LargeMessageCardViewBinder.handleDismissActionButton(mModel);
        verify(mMockDismissActionProvider1, times(1))
                .dismiss(MessageService.MessageType.FOR_TESTING);
    }

    @Test
    @SmallTest
    public void handleDismissActionButton_NoUiProvider() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER, null)
                        .with(
                                MessageCardViewProperties.MESSAGE_TYPE,
                                MessageService.MessageType.FOR_TESTING)
                        .with(
                                MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER,
                                mMockDismissActionProvider1)
                        .build();

        LargeMessageCardViewBinder.handleDismissActionButton(mModel);
        verify(mMockDismissActionProvider1, times(1))
                .dismiss(MessageService.MessageType.FOR_TESTING);
    }

    @Test
    @SmallTest
    public void handleDismissActionButton() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(
                                MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER,
                                mMockDismissActionProvider1)
                        .with(
                                MessageCardViewProperties.MESSAGE_TYPE,
                                MessageService.MessageType.FOR_TESTING)
                        .with(
                                MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER,
                                mMockDismissActionProvider2)
                        .build();

        LargeMessageCardViewBinder.handleDismissActionButton(mModel);
        verify(mMockDismissActionProvider1, times(1))
                .dismiss(MessageService.MessageType.FOR_TESTING);
        verify(mMockDismissActionProvider2, times(1))
                .dismiss(MessageService.MessageType.FOR_TESTING);
    }

    @Test
    @SmallTest
    public void handleReviewActionButton() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(
                                MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER,
                                mMockDismissActionProvider1)
                        .with(
                                MessageCardViewProperties.UI_ACTION_PROVIDER,
                                mMockReviewActionProvider1)
                        .with(
                                MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
                                mMockReviewActionProvider2)
                        .with(
                                MessageCardViewProperties.MESSAGE_TYPE,
                                MessageService.MessageType.FOR_TESTING)
                        .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, false)
                        .build();

        LargeMessageCardViewBinder.handleReviewActionButton(mModel);
        verify(mMockReviewActionProvider1, times(1)).review();
        verify(mMockReviewActionProvider2, times(1)).review();
        verify(mMockDismissActionProvider1, times(1))
                .dismiss(MessageService.MessageType.FOR_TESTING);
    }

    @Test
    @SmallTest
    public void handleReviewActionButton_NoUiProvider() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(
                                MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER,
                                mMockDismissActionProvider1)
                        .with(MessageCardViewProperties.UI_ACTION_PROVIDER, null)
                        .with(
                                MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
                                mMockReviewActionProvider1)
                        .with(
                                MessageCardViewProperties.MESSAGE_TYPE,
                                MessageService.MessageType.FOR_TESTING)
                        .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, false)
                        .build();

        LargeMessageCardViewBinder.handleReviewActionButton(mModel);
        verify(mMockReviewActionProvider1, times(1)).review();
        verify(mMockDismissActionProvider1, times(1))
                .dismiss(MessageService.MessageType.FOR_TESTING);
    }

    @Test
    @SmallTest
    public void handleReviewActionButton_NoServiceProvider() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(
                                MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER,
                                mMockDismissActionProvider1)
                        .with(
                                MessageCardViewProperties.UI_ACTION_PROVIDER,
                                mMockReviewActionProvider1)
                        .with(MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER, null)
                        .with(
                                MessageCardViewProperties.MESSAGE_TYPE,
                                MessageService.MessageType.FOR_TESTING)
                        .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, false)
                        .build();

        LargeMessageCardViewBinder.handleReviewActionButton(mModel);
        verify(mMockReviewActionProvider1, times(1)).review();
        verify(mMockDismissActionProvider1, times(1))
                .dismiss(MessageService.MessageType.FOR_TESTING);
    }

    @Test
    @SmallTest
    public void handleReviewActionButton_NoDismissProvider() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER, null)
                        .with(
                                MessageCardViewProperties.UI_ACTION_PROVIDER,
                                mMockReviewActionProvider1)
                        .with(
                                MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
                                mMockReviewActionProvider2)
                        .with(
                                MessageCardViewProperties.MESSAGE_TYPE,
                                MessageService.MessageType.FOR_TESTING)
                        .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, false)
                        .build();

        LargeMessageCardViewBinder.handleReviewActionButton(mModel);
        verify(mMockReviewActionProvider1, times(1)).review();
        verify(mMockReviewActionProvider2, times(1)).review();
    }

    @Test
    @SmallTest
    public void handleReviewActionButton_DismissNotAllowed() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(
                                MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER,
                                mMockDismissActionProvider1)
                        .with(
                                MessageCardViewProperties.UI_ACTION_PROVIDER,
                                mMockReviewActionProvider1)
                        .with(
                                MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
                                mMockReviewActionProvider2)
                        .with(
                                MessageCardViewProperties.MESSAGE_TYPE,
                                MessageService.MessageType.FOR_TESTING)
                        .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, true)
                        .build();

        LargeMessageCardViewBinder.handleReviewActionButton(mModel);
        verify(mMockReviewActionProvider1, times(1)).review();
        verify(mMockReviewActionProvider2, times(1)).review();
        verify(mMockDismissActionProvider1, never())
                .dismiss(MessageService.MessageType.FOR_TESTING);
    }
}
