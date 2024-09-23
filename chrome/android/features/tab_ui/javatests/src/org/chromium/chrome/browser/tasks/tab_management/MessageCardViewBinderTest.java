// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link MessageCardViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class MessageCardViewBinderTest extends BlankUiTestActivityTestCase {
    private static final String ACTION_TEXT = "actionText";
    private static final String DESCRIPTION_TEXT = "descriptionText";
    private static final String DISMISS_BUTTON_CONTENT_DESCRIPTION = "dismiss";

    private ViewGroup mItemView;
    private PropertyModel mItemViewModel;
    private PropertyModelChangeProcessor mItemMCP;
    private AtomicBoolean mReviewButtonClicked = new AtomicBoolean();
    private AtomicBoolean mDismissButtonClicked = new AtomicBoolean();

    private AtomicBoolean mMessageServiceReviewCallbackRan = new AtomicBoolean();
    private AtomicBoolean mMessageServiceDismissCallbackRan = new AtomicBoolean();

    private MessageCardView.ReviewActionProvider mUiReviewHandler =
            () -> mReviewButtonClicked.set(true);
    private MessageCardView.DismissActionProvider mUiDismissHandler =
            (int messageType) -> mDismissButtonClicked.set(true);
    private MessageCardView.ReviewActionProvider mMessageServiceActionHandler =
            () -> mMessageServiceReviewCallbackRan.set(true);
    private MessageCardView.DismissActionProvider mMessageServiceDismissHandler =
            (int messageType) -> mMessageServiceDismissCallbackRan.set(true);

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ViewGroup view = new LinearLayout(getActivity());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view);

                    mItemView =
                            (ViewGroup)
                                    getActivity()
                                            .getLayoutInflater()
                                            .inflate(R.layout.tab_grid_message_card_item, null);
                    view.addView(mItemView);

                    mItemViewModel =
                            new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                                    .with(MessageCardViewProperties.ACTION_TEXT, ACTION_TEXT)
                                    .with(
                                            MessageCardViewProperties.DESCRIPTION_TEXT,
                                            DESCRIPTION_TEXT)
                                    .build();

                    mItemMCP =
                            PropertyModelChangeProcessor.create(
                                    mItemViewModel, mItemView, MessageCardViewBinder::bind);
                });
    }

    private String getDescriptionText() {
        return ((TextView) mItemView.findViewById(R.id.description)).getText().toString();
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testInitialBinding() {
        assertEquals(
                ACTION_TEXT,
                ((TextView) mItemView.findViewById(R.id.action_button)).getText().toString());
        assertEquals(DESCRIPTION_TEXT, getDescriptionText());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testBindingDescription_WithoutTemplate() {
        mItemViewModel.set(MessageCardViewProperties.DESCRIPTION_TEXT, "test");
        assertEquals("test", getDescriptionText());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testBindingAndClickingReviewHandler() {
        mReviewButtonClicked.set(false);
        mMessageServiceReviewCallbackRan.set(false);
        mItemViewModel.set(MessageCardViewProperties.UI_ACTION_PROVIDER, mUiReviewHandler);
        mItemViewModel.set(
                MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
                mMessageServiceActionHandler);
        mItemViewModel.set(MessageCardViewProperties.ACTION_TEXT, ACTION_TEXT);

        mItemView.findViewById(R.id.action_button).performClick();
        assertTrue(mReviewButtonClicked.get());
        assertTrue(mMessageServiceReviewCallbackRan.get());
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

        mItemView.findViewById(R.id.close_button).performClick();
        assertTrue(mDismissButtonClicked.get());
        assertTrue(mMessageServiceDismissCallbackRan.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetIconVisibility() {
        int margin =
                (int)
                        getActivity()
                                .getResources()
                                .getDimension(R.dimen.tab_grid_iph_item_description_margin);
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams)
                        mItemView.findViewById(R.id.description).getLayoutParams();
        assertEquals(4, mItemView.getChildCount());

        mItemViewModel.set(MessageCardViewProperties.IS_ICON_VISIBLE, false);
        assertEquals(3, mItemView.getChildCount());
        assertEquals(margin, params.leftMargin);

        mItemViewModel.set(MessageCardViewProperties.IS_ICON_VISIBLE, true);
        assertEquals(4, mItemView.getChildCount());
        assertEquals(0, params.leftMargin);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testUpdateMessageCardColor() {
        TextView description = mItemView.findViewById(R.id.description);
        TextView actionButton = mItemView.findViewById(R.id.action_button);
        ImageView closeButton = mItemView.findViewById(R.id.close_button);

        mItemViewModel.set(MessageCardViewProperties.IS_INCOGNITO, false);
        assertThat(
                description.getCurrentTextColor(),
                equalTo(
                        AppCompatResources.getColorStateList(
                                        mItemView.getContext(), R.color.default_text_color_list)
                                .getDefaultColor()));
        assertThat(
                actionButton.getCurrentTextColor(),
                equalTo(SemanticColorUtils.getDefaultTextColorLink(mItemView.getContext())));
        assertThat(
                closeButton.getImageTintList().getDefaultColor(),
                equalTo(getActivity().getColor(R.color.default_icon_color_tint_list)));

        mItemViewModel.set(MessageCardViewProperties.IS_INCOGNITO, true);
        assertThat(
                description.getCurrentTextColor(),
                equalTo(mItemView.getContext().getColor(R.color.default_text_color_light_list)));
        assertThat(
                actionButton.getCurrentTextColor(),
                equalTo(mItemView.getContext().getColor(R.color.default_text_color_link_light)));
        assertThat(
                closeButton.getImageTintList().getDefaultColor(),
                equalTo(getActivity().getColor(R.color.default_icon_color_light)));
    }

    @Override
    public void tearDownTest() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(mItemMCP::destroy);
        super.tearDownTest();
    }
}
