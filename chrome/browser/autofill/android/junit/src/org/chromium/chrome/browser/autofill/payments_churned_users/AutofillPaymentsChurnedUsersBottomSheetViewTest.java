// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.notNullValue;
import static org.robolectric.Shadows.shadowOf;

import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link AutofillPaymentsChurnedUsersBottomSheetView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillPaymentsChurnedUsersBottomSheetViewTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private AutofillPaymentsChurnedUsersBottomSheetView mView;
    private PropertyModel.Builder mModelBuilder;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity ->
                                mView = new AutofillPaymentsChurnedUsersBottomSheetView(activity));
        mModelBuilder =
                new PropertyModel.Builder(
                        AutofillPaymentsChurnedUsersBottomSheetProperties.ALL_KEYS);
    }

    @Test
    public void testViewAccessors() {
        assertThat(mView.getContentView(), notNullValue());
        assertThat(mView.getHeaderIcon(), notNullValue());
        assertThat(mView.getTitleText(), notNullValue());
        assertThat(mView.getHeaderIcon().getId(), equalTo(R.id.payments_churned_users_header_icon));
        assertThat(mView.getTitleText().getId(), equalTo(R.id.payments_churned_users_title));
        assertThat(mView.getDescriptionText(), notNullValue());
        assertThat(
                mView.getDescriptionText().getId(),
                equalTo(R.id.payments_churned_users_description));
        assertThat(mView.getAcceptButton(), notNullValue());
        assertThat(
                mView.getAcceptButton().getId(),
                equalTo(R.id.payments_churned_users_accept_button));
        assertThat(mView.getCancelButton(), notNullValue());
        assertThat(
                mView.getCancelButton().getId(),
                equalTo(R.id.payments_churned_users_cancel_button));
    }

    @Test
    public void testHeaderIcon() {
        bind(
                mModelBuilder.with(
                        AutofillPaymentsChurnedUsersBottomSheetProperties.HEADER_ICON,
                        R.drawable.autofill_payments_churned_users_security_illustration));

        assertThat(mView.getHeaderIcon().getDrawable(), notNullValue());
        assertThat(
                shadowOf(mView.getHeaderIcon().getDrawable()).getCreatedFromResId(),
                equalTo(R.drawable.autofill_payments_churned_users_security_illustration));
        assertThat(mView.getHeaderIcon().getVisibility(), equalTo(View.VISIBLE));
    }

    @Test
    public void testHeaderIcon_hiddenWhenZero() {
        bind(mModelBuilder.with(AutofillPaymentsChurnedUsersBottomSheetProperties.HEADER_ICON, 0));

        assertThat(mView.getHeaderIcon().getVisibility(), equalTo(View.GONE));
    }

    @Test
    public void testTitle() {
        String testTitle = "Check out faster with autofill";
        bind(
                mModelBuilder.with(
                        AutofillPaymentsChurnedUsersBottomSheetProperties.TITLE, testTitle));

        assertThat(mView.getTitleText().getText().toString(), equalTo(testTitle));
    }

    @Test
    public void testDescription() {
        String testDescription = "Turn on autofill to safely check out online.";
        bind(
                mModelBuilder.with(
                        AutofillPaymentsChurnedUsersBottomSheetProperties.DESCRIPTION,
                        testDescription));

        assertThat(mView.getDescriptionText().getText().toString(), equalTo(testDescription));
    }

    @Test
    public void testAcceptButton() {
        String testLabel = "Turn on";
        AtomicBoolean clicked = new AtomicBoolean();
        bind(
                mModelBuilder
                        .with(
                                AutofillPaymentsChurnedUsersBottomSheetProperties
                                        .ACCEPT_BUTTON_LABEL,
                                testLabel)
                        .with(
                                AutofillPaymentsChurnedUsersBottomSheetProperties.ON_ACCEPT_CLICKED,
                                () -> clicked.set(true)));

        assertThat(mView.getAcceptButton().getText().toString(), equalTo(testLabel));
        mView.getAcceptButton().performClick();
        assertThat(clicked.get(), equalTo(true));
    }

    @Test
    public void testCancelButton() {
        String testLabel = "No thanks";
        AtomicBoolean clicked = new AtomicBoolean();
        bind(
                mModelBuilder
                        .with(
                                AutofillPaymentsChurnedUsersBottomSheetProperties
                                        .CANCEL_BUTTON_LABEL,
                                testLabel)
                        .with(
                                AutofillPaymentsChurnedUsersBottomSheetProperties.ON_CANCEL_CLICKED,
                                () -> clicked.set(true)));

        assertThat(mView.getCancelButton().getText().toString(), equalTo(testLabel));
        mView.getCancelButton().performClick();
        assertThat(clicked.get(), equalTo(true));
    }

    private void bind(PropertyModel.Builder modelBuilder) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel = modelBuilder.build();
                    PropertyModelChangeProcessor.create(
                            mModel, mView, AutofillPaymentsChurnedUsersBottomSheetViewBinder::bind);
                });
    }
}
