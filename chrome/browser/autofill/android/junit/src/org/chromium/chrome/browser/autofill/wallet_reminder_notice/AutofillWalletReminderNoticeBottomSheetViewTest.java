// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.wallet_reminder_notice;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.notNullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.autofill.payments.LegalMessage;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link AutofillWalletReminderNoticeBottomSheetView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillWalletReminderNoticeBottomSheetViewTest {
    private Activity mActivity;
    private AutofillWalletReminderNoticeBottomSheetView mView;
    private PropertyModel.Builder mModelBuilder;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView = new AutofillWalletReminderNoticeBottomSheetView(mActivity);
        mModelBuilder =
                new PropertyModel.Builder(
                        AutofillWalletReminderNoticeBottomSheetProperties.ALL_KEYS);
    }

    @Test
    public void testViewAccessors() {
        assertThat(mView.getContentView(), notNullValue());
        assertThat(mView.getTitleText(), notNullValue());
        assertThat(mView.getLegalMessage(), notNullValue());
        assertThat(mView.getGotItButton(), notNullValue());
        assertThat(mView.getTitleText().getId(), equalTo(R.id.wallet_reminder_title));
        assertThat(mView.getLegalMessage().getId(), equalTo(R.id.wallet_reminder_legal_message));
        assertThat(mView.getGotItButton().getId(), equalTo(R.id.wallet_reminder_button_got_it));
    }

    @Test
    public void testHeaderIcon() {
        assertEquals(R.id.wallet_reminder_header_icon, mView.getHeaderIcon().getId());

        bind(
                mModelBuilder.with(
                        AutofillWalletReminderNoticeBottomSheetProperties.HEADER_ICON,
                        R.drawable.autofill_wallet_reminder_notice_illustration));
        assertThat(mView.getHeaderIcon().getDrawable(), notNullValue());
        assertEquals(
                R.drawable.autofill_wallet_reminder_notice_illustration,
                shadowOf(mView.getHeaderIcon().getDrawable()).getCreatedFromResId());
        assertEquals(View.VISIBLE, mView.getHeaderIcon().getVisibility());
    }

    @Test
    public void testHeaderIcon_hiddenWhenZero() {
        bind(mModelBuilder.with(AutofillWalletReminderNoticeBottomSheetProperties.HEADER_ICON, 0));
        assertEquals(View.GONE, mView.getHeaderIcon().getVisibility());
    }

    @Test
    public void testTitle() {
        String testTitle = "Test title for wallet reminder";
        bind(
                mModelBuilder.with(
                        AutofillWalletReminderNoticeBottomSheetProperties.TITLE, testTitle));

        assertThat(mView.getTitleText().getText().toString(), equalTo(testTitle));
    }

    @Test
    public void testLegalMessage() {
        LegalMessageLine line =
                new LegalMessageLine(
                        "Test legal message with a link.",
                        List.of(new LegalMessageLine.Link(27, 31, "https://example.com")));

        bind(
                mModelBuilder.with(
                        AutofillWalletReminderNoticeBottomSheetProperties.LEGAL_MESSAGE,
                        new LegalMessage(List.of(line), (url) -> {})));

        assertThat(mView.getLegalMessage().getText(), notNullValue());
        assertTrue(mView.getLegalMessage().getText().toString().contains("Test legal message"));
        assertEquals(View.VISIBLE, mView.getLegalMessage().getVisibility());
    }

    @Test
    public void testLegalMessage_hiddenWhenEmpty() {
        bind(
                mModelBuilder.with(
                        AutofillWalletReminderNoticeBottomSheetProperties.LEGAL_MESSAGE,
                        new LegalMessage(Collections.emptyList(), (url) -> {})));

        assertEquals(View.GONE, mView.getLegalMessage().getVisibility());
    }

    @Test
    public void testOnGotItClickAction() {
        AtomicBoolean clicked = new AtomicBoolean(false);
        bind(
                mModelBuilder.with(
                        AutofillWalletReminderNoticeBottomSheetProperties.ON_GOT_IT_CLICK_ACTION,
                        () -> clicked.set(true)));

        mView.getGotItButton().performClick();

        assertTrue(clicked.get());
    }

    @Test
    public void testOnGotItClickAction_nullAction() {
        bind(
                mModelBuilder.with(
                        AutofillWalletReminderNoticeBottomSheetProperties.ON_GOT_IT_CLICK_ACTION,
                        null));

        mView.getGotItButton().performClick();
    }

    private void bind(PropertyModel.Builder modelBuilder) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel = modelBuilder.build();
                    PropertyModelChangeProcessor.create(
                            mModel, mView, AutofillWalletReminderNoticeBottomSheetViewBinder::bind);
                });
    }
}
