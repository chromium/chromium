// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Batch.UNIT_TESTS;

import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link LargeMessageCardViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(UNIT_TESTS)
public class LargeMessageCardViewBinderTest extends BlankUiTestActivityTestCase {
    private static final String TITLE_TEXT = "titleText";
    private static final String ACTION_TEXT = "actionText";
    private static final String DESCRIPTION_TEXT = "descriptionText";
    private static final String SECONDARY_ACTION_TEXT = "secondaryActionText";
    private static final String DISMISS_BUTTON_CONTENT_DESCRIPTION = "dismiss";
    private static final String PRICE = "$300";
    private static final String PREVIOUS_PRICE = "$400";

    private final AtomicBoolean mReviewButtonClicked = new AtomicBoolean();
    private final AtomicBoolean mSecondaryActionButtonClicked = new AtomicBoolean();
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
    private final OnClickListener mSecondaryActionButtonClickListener =
            view -> mSecondaryActionButtonClicked.set(true);

    private ChromeImageView mIcon;
    private TextView mTitle;
    private TextView mDescription;
    private ButtonCompat mActionButton;
    private ButtonCompat mSecondaryActionButton;
    private ChromeImageView mCloseButton;
    private PriceCardView mPriceInfoBox;
    private LargeMessageCardView mItemView;
    private PropertyModel mItemViewModel;
    private PropertyModelChangeProcessor mItemMCP;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ViewGroup view = new FrameLayout(getActivity());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view);

                    mItemView =
                            (LargeMessageCardView)
                                    getActivity()
                                            .getLayoutInflater()
                                            .inflate(R.layout.large_message_card_item, null);
                    view.addView(mItemView);

                    mPriceInfoBox = mItemView.findViewById(R.id.price_info_box);
                    mIcon = mItemView.findViewById(R.id.icon);
                    mTitle = mItemView.findViewById(R.id.title);
                    mDescription = mItemView.findViewById(R.id.description);
                    mActionButton = mItemView.findViewById(R.id.action_button);
                    mSecondaryActionButton = mItemView.findViewById(R.id.secondary_action_button);
                    mCloseButton = mItemView.findViewById(R.id.close_button);

                    mItemViewModel =
                            new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                                    .with(MessageCardViewProperties.TITLE_TEXT, TITLE_TEXT)
                                    .with(MessageCardViewProperties.ACTION_TEXT, ACTION_TEXT)
                                    .with(
                                            MessageCardViewProperties.DESCRIPTION_TEXT,
                                            DESCRIPTION_TEXT)
                                    .with(
                                            MessageCardViewProperties
                                                    .DISMISS_BUTTON_CONTENT_DESCRIPTION,
                                            DISMISS_BUTTON_CONTENT_DESCRIPTION)
                                    .with(MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW, false)
                                    .build();

                    mItemMCP =
                            PropertyModelChangeProcessor.create(
                                    mItemViewModel, mItemView, LargeMessageCardViewBinder::bind);
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
        assertEquals(
                "Secondary action button should be gone by default.",
                GONE,
                mSecondaryActionButton.getVisibility());
        assertEquals(
                "Close button should be visible by default.",
                VISIBLE,
                mCloseButton.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetIconDrawable() {
        assertNull(mIcon.getDrawable());

        Drawable iconDrawable =
                AppCompatResources.getDrawable(getActivity(), R.drawable.ic_globe_24dp);
        mItemViewModel.set(
                MessageCardViewProperties.ICON_PROVIDER,
                (callback) -> {
                    callback.onResult(iconDrawable);
                });
        assertEquals(iconDrawable, mIcon.getDrawable());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetIconVisibility() {
        assertEquals(GONE, mIcon.getVisibility());

        mItemViewModel.set(MessageCardViewProperties.IS_ICON_VISIBLE, true);
        assertEquals(VISIBLE, mIcon.getVisibility());
        mItemViewModel.set(MessageCardViewProperties.IS_ICON_VISIBLE, false);
        assertEquals(GONE, mIcon.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetCloseIconVisibility() {
        mItemViewModel.set(MessageCardViewProperties.IS_CLOSE_BUTTON_VISIBLE, false);
        assertEquals("Close button should not be visible.", GONE, mCloseButton.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSecondaryActionText() {
        mItemViewModel.set(MessageCardViewProperties.SECONDARY_ACTION_TEXT, SECONDARY_ACTION_TEXT);
        assertEquals(
                "Fail to set secondary action text.",
                SECONDARY_ACTION_TEXT,
                mSecondaryActionButton.getText());
        assertEquals(
                "Secondary action text should be visible.",
                View.VISIBLE,
                mSecondaryActionButton.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSecondaryActionOnClickListenerTest() {
        mSecondaryActionButtonClicked.set(false);
        mItemViewModel.set(MessageCardViewProperties.SECONDARY_ACTION_TEXT, SECONDARY_ACTION_TEXT);
        mItemViewModel.set(
                MessageCardViewProperties.SECONDARY_ACTION_BUTTON_CLICK_HANDLER,
                mSecondaryActionButtonClickListener);
        mSecondaryActionButton.performClick();
        assertTrue("Secondary button didn't fire properly.", mSecondaryActionButtonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetupPriceInfoBox() {
        assertEquals(GONE, mPriceInfoBox.getVisibility());

        mItemViewModel.set(
                MessageCardViewProperties.PRICE_DROP,
                new ShoppingPersistedTabData.PriceDrop(PRICE, PREVIOUS_PRICE));
        assertEquals(VISIBLE, mPriceInfoBox.getVisibility());
        assertEquals(
                PRICE,
                ((TextView) mItemView.findViewById(R.id.current_price)).getText().toString());
        assertEquals(
                PREVIOUS_PRICE,
                ((TextView) mItemView.findViewById(R.id.previous_price)).getText().toString());

        mItemViewModel.set(MessageCardViewProperties.PRICE_DROP, null);
        assertEquals(GONE, mPriceInfoBox.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testBindingAndClickingReviewHandler() {
        mReviewButtonClicked.set(false);
        mMessageServiceReviewCallbackRan.set(false);
        mDismissButtonClicked.set(false);
        mItemViewModel.set(MessageCardViewProperties.UI_ACTION_PROVIDER, mUiReviewHandler);
        mItemViewModel.set(
                MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
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
        mItemViewModel.set(
                MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER,
                mMessageServiceDismissHandler);
        mItemViewModel.set(
                MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION,
                DISMISS_BUTTON_CONTENT_DESCRIPTION);

        mCloseButton.performClick();
        assertTrue(mDismissButtonClicked.get());
        assertTrue(mMessageServiceDismissCallbackRan.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testUpdateWidthWithOrientation() {
        mItemView.setPadding(0, 0, 0, 0);
        mItemView.updateWidthWithOrientation(Configuration.ORIENTATION_PORTRAIT);
        assertEquals(0, mItemView.getPaddingLeft());
        assertEquals(0, mItemView.getPaddingTop());
        assertEquals(0, mItemView.getPaddingRight());
        assertEquals(0, mItemView.getPaddingBottom());

        int landscapeSidePadding =
                (int)
                        getActivity()
                                .getResources()
                                .getDimension(
                                        R.dimen.tab_grid_large_message_side_padding_landscape);
        mItemView.setPadding(0, 0, 0, 0);
        mItemView.updateWidthWithOrientation(Configuration.ORIENTATION_LANDSCAPE);
        assertEquals(landscapeSidePadding, mItemView.getPaddingLeft());
        assertEquals(0, mItemView.getPaddingTop());
        assertEquals(landscapeSidePadding, mItemView.getPaddingRight());
        assertEquals(0, mItemView.getPaddingBottom());
    }

    @Override
    public void tearDownTest() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(mItemMCP::destroy);
        super.tearDownTest();
    }
}
