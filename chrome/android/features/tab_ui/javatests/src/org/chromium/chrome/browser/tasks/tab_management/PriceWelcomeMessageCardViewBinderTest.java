// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link PriceWelcomeMessageCardViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class PriceWelcomeMessageCardViewBinderTest extends DummyUiActivityTestCase {
    private static final String TITLE_TEXT = "titleText";
    private static final String ACTION_TEXT = "actionText";
    private static final String DESCRIPTION_TEXT = "descriptionText";
    private static final String DISMISS_BUTTON_CONTENT_DESCRIPTION = "dismiss";
    private static final String PRICE = "$300";
    private static final String PREVIOUS_PRICE = "$400";

    private final AtomicBoolean mReviewButtonClicked = new AtomicBoolean();
    private final AtomicBoolean mDismissButtonClicked = new AtomicBoolean();
    private final AtomicBoolean mMessageServiceReviewCallbackRan = new AtomicBoolean();
    private final AtomicBoolean mMessageServiceDismissCallbackRan = new AtomicBoolean();

    private final MessageCardView.ReviewActionProvider mUiReviewHandler =
            () -> mReviewButtonClicked.set(true);
    private final MessageCardView.DismissActionProvider mUiDismissHandler =
            (int messageType) -> mDismissButtonClicked.set(true);
    private final MessageCardView.ReviewActionProvider mMessageServiceActionHandler =
            () -> mMessageServiceReviewCallbackRan.set(true);
    private final MessageCardView.DismissActionProvider mMessageServiceDismissHandler =
            (int messageType) -> mMessageServiceDismissCallbackRan.set(true);

    private TextView mTitle;
    private TextView mDescription;
    private ButtonCompat mActionButton;
    private ChromeImageView mCloseButton;
    private ViewGroup mItemView;
    private PropertyModel mItemViewModel;
    private PropertyModelChangeProcessor mItemMCP;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ViewGroup view = new FrameLayout(getActivity());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().setContentView(view);

            mItemView = (ViewGroup) getActivity().getLayoutInflater().inflate(
                    R.layout.price_welcome_message_card_item, null);
            view.addView(mItemView);

            mTitle = mItemView.findViewById(R.id.title);
            mDescription = mItemView.findViewById(R.id.content);
            mActionButton = mItemView.findViewById(R.id.action_button);
            mCloseButton = mItemView.findViewById(R.id.close_button);

            mItemViewModel =
                    new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                            .with(MessageCardViewProperties.TITLE_TEXT, TITLE_TEXT)
                            .with(MessageCardViewProperties.ACTION_TEXT, ACTION_TEXT)
                            .with(MessageCardViewProperties.DESCRIPTION_TEXT, DESCRIPTION_TEXT)
                            .with(MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION,
                                    DISMISS_BUTTON_CONTENT_DESCRIPTION)
                            .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, false)
                            .build();

            mItemMCP = PropertyModelChangeProcessor.create(
                    mItemViewModel, mItemView, PriceWelcomeMessageCardViewBinder::bind);
        });
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testInitialBinding() {
        assertEquals(TITLE_TEXT, mTitle.getText().toString());
        assertEquals(ACTION_TEXT, mActionButton.getText().toString());
        assertEquals(DESCRIPTION_TEXT, mDescription.getText().toString());
        assertEquals(DISMISS_BUTTON_CONTENT_DESCRIPTION, mCloseButton.getContentDescription());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetPriceInfoBoxStrings() {
        mItemViewModel.set(MessageCardViewProperties.PRICE_DROP,
                new ShoppingPersistedTabData.PriceDrop(PRICE, PREVIOUS_PRICE));
        assertEquals(PRICE,
                ((TextView) mItemView.findViewById(R.id.current_price)).getText().toString());
        assertEquals(PREVIOUS_PRICE,
                ((TextView) mItemView.findViewById(R.id.previous_price)).getText().toString());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testBindingAndClickingReviewHandler() {
        mReviewButtonClicked.set(false);
        mMessageServiceReviewCallbackRan.set(false);
        mDismissButtonClicked.set(false);
        mItemViewModel.set(MessageCardViewProperties.UI_ACTION_PROVIDER, mUiReviewHandler);
        mItemViewModel.set(MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
                mMessageServiceActionHandler);
        mItemViewModel.set(MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER, mUiDismissHandler);
        mItemViewModel.set(MessageCardViewProperties.ACTION_TEXT, ACTION_TEXT);

        mActionButton.performClick();
        assertTrue(mReviewButtonClicked.get());
        assertTrue(mMessageServiceReviewCallbackRan.get());
        assertTrue(mDismissButtonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testBindingAndClickingDismissHandler() {
        mDismissButtonClicked.set(false);
        mMessageServiceDismissCallbackRan.set(false);
        mItemViewModel.set(MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER, mUiDismissHandler);
        mItemViewModel.set(MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER,
                mMessageServiceDismissHandler);
        mItemViewModel.set(MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION,
                DISMISS_BUTTON_CONTENT_DESCRIPTION);

        mCloseButton.performClick();
        assertTrue(mDismissButtonClicked.get());
        assertTrue(mMessageServiceDismissCallbackRan.get());
    }

    @Override
    public void tearDownTest() throws Exception {
        mItemMCP.destroy();
        super.tearDownTest();
    }
}
