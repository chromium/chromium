// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link LargeMessageCardViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LargeMessageCardViewBinderUnitTest {
    private static final String FAKE_DISPLAY_TEXT = "Fake Text";
    private static final int FAKE_ICON_WIDTH = 666;
    private static final int FAKE_ICON_HEIGHT = 999;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock MessageCardView.ActionProvider mMockDismissActionProvider1;

    @Mock
    MessageCardView.ServiceDismissActionProvider<@MessageType Integer>
            mMockServiceDismissActionProvider2;

    @Mock MessageCardView.ActionProvider mMockActionProvider1;

    @Mock MessageCardView.ActionProvider mMockActionProvider2;

    @Mock MessageCardView.IconProvider mMockIconProvider;

    @Mock private OnClickListener mMockOnClickListenerMock;

    @Captor ArgumentCaptor<Callback<Drawable>> mCallbackDrawableArgumentCaptor;

    private LargeMessageCardView mLargeCardView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mLargeCardView =
                (LargeMessageCardView)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.large_message_card_item, null);

        mModel = new PropertyModel(MessageCardViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mModel, mLargeCardView, LargeMessageCardViewBinder::bind);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(
                mMockDismissActionProvider1,
                mMockServiceDismissActionProvider2,
                mMockActionProvider1,
                mMockActionProvider2,
                mMockIconProvider,
                mMockOnClickListenerMock);
    }

    @Test
    public void updateContntDescriptionText() {
        mModel.set(
                MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER, mMockDismissActionProvider1);
        View closeButton = mLargeCardView.findViewById(R.id.close_button);

        mModel.set(MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION, FAKE_DISPLAY_TEXT);
        assertEquals(FAKE_DISPLAY_TEXT, closeButton.getContentDescription());
        closeButton.performClick();
        verify(mMockDismissActionProvider1, times(1)).action();

        mModel.set(MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION, null);
        assertNull(closeButton.getContentDescription());
        closeButton.performClick();
        verify(mMockDismissActionProvider1, times(2)).action();
    }

    @Test
    public void updateActionButtonText() {
        mModel.set(MessageCardViewProperties.UI_ACTION_PROVIDER, mMockActionProvider1);
        TextView actionButton = mLargeCardView.findViewById(R.id.action_button);

        mModel.set(MessageCardViewProperties.ACTION_TEXT, FAKE_DISPLAY_TEXT);
        assertEquals(FAKE_DISPLAY_TEXT, actionButton.getText().toString());
        actionButton.performClick();
        verify(mMockActionProvider1, times(1)).action();

        mModel.set(MessageCardViewProperties.ACTION_TEXT, null);
        assertEquals("", actionButton.getText().toString());
        actionButton.performClick();
        verify(mMockActionProvider1, times(2)).action();
    }

    @Test
    public void updateSecondaryActionButtonText() {
        TextView secondaryActionButton = mLargeCardView.findViewById(R.id.secondary_action_button);
        mModel.set(MessageCardViewProperties.SECONDARY_ACTION_TEXT, FAKE_DISPLAY_TEXT);
        assertEquals(FAKE_DISPLAY_TEXT, secondaryActionButton.getText().toString());
        assertEquals(View.VISIBLE, secondaryActionButton.getVisibility());
    }

    @Test
    public void updateSecondaryActionButtonOnClickListener() {
        mModel.set(
                MessageCardViewProperties.SECONDARY_ACTION_BUTTON_CLICK_HANDLER,
                mMockOnClickListenerMock);
        View secondaryActionButton = mLargeCardView.findViewById(R.id.secondary_action_button);
        secondaryActionButton.performClick();
        verify(mMockOnClickListenerMock, times(1)).onClick(secondaryActionButton);
    }

    @Test
    public void updateTitleText() {
        TextView title = mLargeCardView.findViewById(R.id.title);
        mModel.set(MessageCardViewProperties.TITLE_TEXT, FAKE_DISPLAY_TEXT);
        assertEquals(FAKE_DISPLAY_TEXT, title.getText().toString());

        mModel.set(MessageCardViewProperties.TITLE_TEXT, null);
        assertEquals("", title.getText().toString());
    }

    @Test
    public void updatePriceDropInfo() {
        View priceInfoBox = mLargeCardView.findViewById(R.id.price_info_box);
        mModel.set(
                MessageCardViewProperties.PRICE_DROP,
                new ShoppingPersistedTabData.PriceDrop("$5", "$10"));
        assertEquals(View.VISIBLE, priceInfoBox.getVisibility());
    }

    @Test
    public void updateIconVisibility() {
        View icon = mLargeCardView.findViewById(R.id.icon);
        mModel.set(MessageCardViewProperties.IS_ICON_VISIBLE, true);
        assertEquals(View.VISIBLE, icon.getVisibility());

        mModel.set(MessageCardViewProperties.IS_ICON_VISIBLE, false);
        assertEquals(View.GONE, icon.getVisibility());
    }

    @Test
    public void updateIconWidth() {
        mModel.set(MessageCardViewProperties.ICON_WIDTH_IN_PIXELS, FAKE_ICON_WIDTH);
        assertEquals(
                FAKE_ICON_WIDTH, mLargeCardView.findViewById(R.id.icon).getLayoutParams().width);
    }

    @Test
    public void updateIconHeight() {
        mModel.set(MessageCardViewProperties.ICON_HEIGHT_IN_PIXELS, FAKE_ICON_HEIGHT);
        assertEquals(
                FAKE_ICON_HEIGHT, mLargeCardView.findViewById(R.id.icon).getLayoutParams().height);
    }

    @Test
    public void updateCloseIconVisibility() {
        View closeButton = mLargeCardView.findViewById(R.id.close_button);
        mModel.set(MessageCardViewProperties.IS_CLOSE_BUTTON_VISIBLE, false);
        assertEquals(View.GONE, closeButton.getVisibility());

        mModel.set(MessageCardViewProperties.IS_CLOSE_BUTTON_VISIBLE, true);
        assertEquals(View.VISIBLE, closeButton.getVisibility());
    }

    @Test
    public void updateIconDrawable() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(MessageCardViewProperties.ICON_PROVIDER, mMockIconProvider)
                        .build();

        LargeMessageCardViewBinder.updateIconDrawable(mModel, mLargeCardView);
        verify(mMockIconProvider).fetchIconDrawable(mCallbackDrawableArgumentCaptor.capture());
        Callback<Drawable> callback = mCallbackDrawableArgumentCaptor.getValue();
        Drawable drawable = new ColorDrawable(Color.RED);
        callback.onResult(drawable);
        ImageView icon = mLargeCardView.findViewById(R.id.icon);
        assertEquals(drawable, icon.getDrawable());
    }

    @Test
    public void updateCardAlpha() {
        mModel.set(CARD_ALPHA, 0.5f);
        assertEquals(0.5f, mLargeCardView.getAlpha(), 0f);
    }

    @Test
    public void handleDismissActionButton_NoServiceProvider() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(
                                MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER,
                                mMockDismissActionProvider1)
                        .with(MessageCardViewProperties.MESSAGE_TYPE, MessageType.FOR_TESTING)
                        .with(
                                MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER,
                                null)
                        .build();

        LargeMessageCardViewBinder.handleDismissActionButton(mModel);
        verify(mMockDismissActionProvider1, times(1)).action();
    }

    @Test
    public void handleDismissActionButton_NoUiProvider() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(
                                MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER,
                                null)
                        .with(MessageCardViewProperties.MESSAGE_TYPE, MessageType.FOR_TESTING)
                        .with(
                                MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER,
                                mMockDismissActionProvider1)
                        .build();

        LargeMessageCardViewBinder.handleDismissActionButton(mModel);
        verify(mMockDismissActionProvider1, times(1)).action();
    }

    @Test
    public void handleDismissActionButton() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(
                                MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER,
                                mMockDismissActionProvider1)
                        .with(MessageCardViewProperties.MESSAGE_TYPE, MessageType.FOR_TESTING)
                        .with(
                                MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER,
                                mMockServiceDismissActionProvider2)
                        .build();

        LargeMessageCardViewBinder.handleDismissActionButton(mModel);
        verify(mMockDismissActionProvider1, times(1)).action();
        verify(mMockServiceDismissActionProvider2, times(1)).dismiss(MessageType.FOR_TESTING);
    }

    @Test
    public void handleReviewActionButton() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(
                                MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER,
                                mMockDismissActionProvider1)
                        .with(MessageCardViewProperties.UI_ACTION_PROVIDER, mMockActionProvider1)
                        .with(
                                MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
                                mMockActionProvider2)
                        .with(MessageCardViewProperties.MESSAGE_TYPE, MessageType.FOR_TESTING)
                        .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, false)
                        .build();

        LargeMessageCardViewBinder.handleReviewActionButton(mModel);
        verify(mMockActionProvider1, times(1)).action();
        verify(mMockActionProvider2, times(1)).action();
        verify(mMockDismissActionProvider1, times(1)).action();
    }

    @Test
    public void handleReviewActionButton_NoUiProvider() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(
                                MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER,
                                mMockDismissActionProvider1)
                        .with(MessageCardViewProperties.UI_ACTION_PROVIDER, null)
                        .with(
                                MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
                                mMockActionProvider1)
                        .with(MessageCardViewProperties.MESSAGE_TYPE, MessageType.FOR_TESTING)
                        .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, false)
                        .build();

        LargeMessageCardViewBinder.handleReviewActionButton(mModel);
        verify(mMockActionProvider1, times(1)).action();
        verify(mMockDismissActionProvider1, times(1)).action();
    }

    @Test
    public void handleReviewActionButton_NoServiceProvider() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(
                                MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER,
                                mMockDismissActionProvider1)
                        .with(MessageCardViewProperties.UI_ACTION_PROVIDER, mMockActionProvider1)
                        .with(MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER, null)
                        .with(MessageCardViewProperties.MESSAGE_TYPE, MessageType.FOR_TESTING)
                        .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, false)
                        .build();

        LargeMessageCardViewBinder.handleReviewActionButton(mModel);
        verify(mMockActionProvider1, times(1)).action();
        verify(mMockDismissActionProvider1, times(1)).action();
    }

    @Test
    public void handleReviewActionButton_NoDismissProvider() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER, null)
                        .with(MessageCardViewProperties.UI_ACTION_PROVIDER, mMockActionProvider1)
                        .with(
                                MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
                                mMockActionProvider2)
                        .with(MessageCardViewProperties.MESSAGE_TYPE, MessageType.FOR_TESTING)
                        .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, false)
                        .build();

        LargeMessageCardViewBinder.handleReviewActionButton(mModel);
        verify(mMockActionProvider1, times(1)).action();
        verify(mMockActionProvider2, times(1)).action();
    }

    @Test
    public void handleReviewActionButton_DismissNotAllowed() {
        mModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(
                                MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER,
                                mMockDismissActionProvider1)
                        .with(MessageCardViewProperties.UI_ACTION_PROVIDER, mMockActionProvider1)
                        .with(
                                MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
                                mMockActionProvider2)
                        .with(MessageCardViewProperties.MESSAGE_TYPE, MessageType.FOR_TESTING)
                        .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, true)
                        .build();

        LargeMessageCardViewBinder.handleReviewActionButton(mModel);
        verify(mMockActionProvider1, times(1)).action();
        verify(mMockActionProvider2, times(1)).action();
        verify(mMockDismissActionProvider1, never()).action();
    }
}
