// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link MessageCardViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class MessageCardViewBinderTest extends DummyUiActivityTestCase {
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

    private MessageCardView.ReviewActionProvider mUiReviewHandler = () -> {
        mReviewButtonClicked.set(true);
    };
    private MessageCardView.DismissActionProvider mUiDismissHandler = () -> {
        mDismissButtonClicked.set(true);
    };
    private MessageCardView.ReviewActionProvider mMessageServiceActionHandler = () -> {
        mMessageServiceReviewCallbackRan.set(true);
    };
    private MessageCardView.DismissActionProvider mMessageServiceDismissHandler = () -> {
        mMessageServiceDismissCallbackRan.set(true);
    };

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ViewGroup view = new LinearLayout(getActivity());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().setContentView(view);

            mItemView = (ViewGroup) getActivity().getLayoutInflater().inflate(
                    R.layout.tab_grid_message_card_item, null);
            view.addView(mItemView);
        });

        mItemViewModel = new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                                 .with(MessageCardViewProperties.ACTION_TEXT, ACTION_TEXT)
                                 .with(MessageCardViewProperties.DESCRIPTION_TEXT, DESCRIPTION_TEXT)
                                 .build();

        mItemMCP = PropertyModelChangeProcessor.create(
                mItemViewModel, mItemView, MessageCardViewBinder::bind);
    }

    private String getDescriptionText() {
        return ((TextView) mItemView.findViewById(R.id.description)).getText().toString();
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testInitialBinding() {
        assertEquals(ACTION_TEXT,
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
    public void testBindingDescription_WithTemplate() {
        mItemViewModel.set(MessageCardViewProperties.DESCRIPTION_TEXT_TEMPLATE, "%s template");
        mItemViewModel.set(MessageCardViewProperties.DESCRIPTION_TEXT, "test");
        assertEquals("test template", getDescriptionText());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testBindingAndClickingReviewHandler() {
        mReviewButtonClicked.set(false);
        mMessageServiceReviewCallbackRan.set(false);
        mItemViewModel.set(MessageCardViewProperties.UI_ACTION_PROVIDER, mUiReviewHandler);
        mItemViewModel.set(MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER,
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
        mItemViewModel.set(MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER,
                mMessageServiceDismissHandler);
        mItemViewModel.set(MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION,
                DISMISS_BUTTON_CONTENT_DESCRIPTION);

        mItemView.findViewById(R.id.close_button).performClick();
        assertTrue(mDismissButtonClicked.get());
        assertTrue(mMessageServiceDismissCallbackRan.get());
    }

    @Override
    public void tearDownTest() throws Exception {
        mItemMCP.destroy();
        super.tearDownTest();
    }
}
