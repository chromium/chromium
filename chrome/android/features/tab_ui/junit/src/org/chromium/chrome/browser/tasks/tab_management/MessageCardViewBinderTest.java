// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.OutlineOverlayHelper;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link MessageCardViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageCardViewBinderTest {
    private static final String ACTION_TEXT = "actionText";
    private static final String DESCRIPTION_TEXT = "descriptionText";
    private static final String DISMISS_BUTTON_CONTENT_DESCRIPTION = "dismiss";
    private static final int MARGIN_OVERRIDE = 10;

    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private final AtomicBoolean mReviewButtonClicked = new AtomicBoolean();
    private final AtomicBoolean mDismissButtonClicked = new AtomicBoolean();

    private final AtomicBoolean mMessageServiceReviewCallbackRan = new AtomicBoolean();
    private final AtomicBoolean mMessageServiceDismissCallbackRan = new AtomicBoolean();

    private final MessageCardView.ActionProvider mUiReviewHandler =
            () -> mReviewButtonClicked.set(true);
    private final MessageCardView.ActionProvider mUiDismissHandler =
            () -> mDismissButtonClicked.set(true);
    private final MessageCardView.ActionProvider mMessageServiceActionHandler =
            () -> mMessageServiceReviewCallbackRan.set(true);
    private final MessageCardView.ServiceDismissActionProvider<@MessageType Integer>
            mMessageServiceDismissHandler =
                    messageType -> mMessageServiceDismissCallbackRan.set(true);

    private Activity mActivity;
    private ViewGroup mItemView;
    private PropertyModel mItemViewModel;
    private PropertyModelChangeProcessor mItemMCP;

    @Before
    public void setUp() throws Exception {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            ViewGroup view = new LinearLayout(mActivity);
                            mActivity.setContentView(view);

                            mItemView =
                                    (ViewGroup)
                                            mActivity
                                                    .getLayoutInflater()
                                                    .inflate(
                                                            R.layout.tab_grid_message_card_item,
                                                            null);
                            view.addView(mItemView);

                            mItemViewModel =
                                    new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                                            .with(
                                                    MessageCardViewProperties.ACTION_TEXT,
                                                    ACTION_TEXT)
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
    public void testInitialBinding() {
        assertEquals(
                ACTION_TEXT,
                ((TextView) mItemView.findViewById(R.id.action_button)).getText().toString());
        assertEquals(DESCRIPTION_TEXT, getDescriptionText());
    }

    @Test
    public void testBindingDescription_WithoutTemplate() {
        mItemViewModel.set(MessageCardViewProperties.DESCRIPTION_TEXT, "test");
        assertEquals("test", getDescriptionText());
    }

    @Test
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
    public void testSetIconVisibility() {
        int margin =
                (int)
                        mActivity
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
    public void testUpdateMessageCardColor() {
        TextView description = mItemView.findViewById(R.id.description);
        TextView actionButton = mItemView.findViewById(R.id.action_button);
        ImageView closeButton = mItemView.findViewById(R.id.close_button);

        mItemViewModel.set(MessageCardViewProperties.IS_INCOGNITO, false);
        assertThat(
                description.getCurrentTextColor(),
                equalTo(
                        mItemView
                                .getContext()
                                .getColorStateList(R.color.default_text_color_list)
                                .getDefaultColor()));
        assertThat(
                actionButton.getCurrentTextColor(),
                equalTo(SemanticColorUtils.getDefaultTextColorLink(mItemView.getContext())));
        assertThat(
                closeButton.getImageTintList().getDefaultColor(),
                equalTo(mActivity.getColor(R.color.default_icon_color_tint_list)));

        mItemViewModel.set(MessageCardViewProperties.IS_INCOGNITO, true);
        assertThat(
                description.getCurrentTextColor(),
                equalTo(mItemView.getContext().getColor(R.color.default_text_color_light_list)));
        assertThat(
                actionButton.getCurrentTextColor(),
                equalTo(mItemView.getContext().getColor(R.color.default_text_color_link_light)));
        assertThat(
                closeButton.getImageTintList().getDefaultColor(),
                equalTo(mActivity.getColor(R.color.default_icon_color_light)));
    }

    @Test
    public void testSetLeftMargin() {
        View messageCardView = mItemView.findViewById(R.id.tab_grid_message_item);
        mItemViewModel.set(MessageCardViewProperties.LEFT_MARGIN_OVERRIDE_PX, MARGIN_OVERRIDE);
        ViewGroup.MarginLayoutParams oldParams =
                (ViewGroup.MarginLayoutParams) messageCardView.getLayoutParams();

        ViewGroup.MarginLayoutParams newParams =
                (ViewGroup.MarginLayoutParams) messageCardView.getLayoutParams();
        assertEquals(MARGIN_OVERRIDE, newParams.leftMargin);
        assertEquals(oldParams.topMargin, newParams.topMargin);
        assertEquals(oldParams.rightMargin, newParams.rightMargin);
        assertEquals(oldParams.bottomMargin, newParams.bottomMargin);
    }

    @Test
    public void testSetTopMargin() {
        View messageCardView = mItemView.findViewById(R.id.tab_grid_message_item);
        mItemViewModel.set(MessageCardViewProperties.TOP_MARGIN_OVERRIDE_PX, MARGIN_OVERRIDE);
        ViewGroup.MarginLayoutParams oldParams =
                (ViewGroup.MarginLayoutParams) messageCardView.getLayoutParams();

        ViewGroup.MarginLayoutParams newParams =
                (ViewGroup.MarginLayoutParams) messageCardView.getLayoutParams();
        assertEquals(oldParams.leftMargin, newParams.leftMargin);
        assertEquals(MARGIN_OVERRIDE, newParams.topMargin);
        assertEquals(oldParams.rightMargin, newParams.rightMargin);
        assertEquals(oldParams.bottomMargin, newParams.bottomMargin);
    }

    @Test
    public void testSetRightMargin() {
        View messageCardView = mItemView.findViewById(R.id.tab_grid_message_item);
        mItemViewModel.set(MessageCardViewProperties.RIGHT_MARGIN_OVERRIDE_PX, MARGIN_OVERRIDE);
        ViewGroup.MarginLayoutParams oldParams =
                (ViewGroup.MarginLayoutParams) messageCardView.getLayoutParams();

        ViewGroup.MarginLayoutParams newParams =
                (ViewGroup.MarginLayoutParams) messageCardView.getLayoutParams();
        assertEquals(oldParams.leftMargin, newParams.leftMargin);
        assertEquals(oldParams.topMargin, newParams.topMargin);
        assertEquals(MARGIN_OVERRIDE, newParams.rightMargin);
        assertEquals(oldParams.bottomMargin, newParams.bottomMargin);
    }

    @Test
    public void testSetBottomMargin() {
        View messageCardView = mItemView.findViewById(R.id.tab_grid_message_item);
        mItemViewModel.set(MessageCardViewProperties.BOTTOM_MARGIN_OVERRIDE_PX, MARGIN_OVERRIDE);
        ViewGroup.MarginLayoutParams oldParams =
                (ViewGroup.MarginLayoutParams) messageCardView.getLayoutParams();

        ViewGroup.MarginLayoutParams newParams =
                (ViewGroup.MarginLayoutParams) messageCardView.getLayoutParams();
        assertEquals(oldParams.leftMargin, newParams.leftMargin);
        assertEquals(oldParams.topMargin, newParams.topMargin);
        assertEquals(oldParams.rightMargin, newParams.rightMargin);
        assertEquals(MARGIN_OVERRIDE, newParams.bottomMargin);
    }

    @Test
    public void testFocusOutline() {
        View messageCardView = mItemView.findViewById(R.id.tab_grid_message_item);

        // Test only: make the view focusable in touch mode to mimic accessories
        // are attached.
        messageCardView.setFocusableInTouchMode(true);
        messageCardView.requestFocus();

        assertTrue("Focus is correctly registered.", messageCardView.isFocused());

        GradientDrawable drawable =
                (GradientDrawable)
                        OutlineOverlayHelper.getFocusOutlineDrawableForTesting(messageCardView);
        assertNotNull("Focus outline drawable is null.", drawable);

        int outlineColor =
                ChromeColors.getKeyboardFocusRingColor(messageCardView.getContext(), false);
        assertEquals(
                "Regular outline color is wrong.",
                drawable.getColorFilter(),
                new PorterDuffColorFilter(outlineColor, Mode.SRC_IN));

        mItemViewModel.set(MessageCardViewProperties.IS_INCOGNITO, true);
        drawable =
                (GradientDrawable)
                        OutlineOverlayHelper.getFocusOutlineDrawableForTesting(messageCardView);
        assertNotNull("Focus outline drawable is null.", drawable);

        int incognitoOutlineColor =
                ChromeColors.getKeyboardFocusRingColor(messageCardView.getContext(), true);
        assertEquals(
                "Incognito outline color is wrong.",
                drawable.getColorFilter(),
                new PorterDuffColorFilter(incognitoOutlineColor, Mode.SRC_IN));
    }

    @After
    public void tearDown() throws Exception {
        mItemMCP.destroy();
    }
}
