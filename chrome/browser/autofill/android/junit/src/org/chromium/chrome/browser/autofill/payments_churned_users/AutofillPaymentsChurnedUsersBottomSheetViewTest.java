// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.notNullValue;

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
        assertThat(mView.getTitleText(), notNullValue());
        assertThat(mView.getTitleText().getId(), equalTo(R.id.payments_churned_users_title));
    }

    @Test
    public void testTitle() {
        String testTitle = "Check out faster with autofill";
        bind(
                mModelBuilder.with(
                        AutofillPaymentsChurnedUsersBottomSheetProperties.TITLE, testTitle));

        assertThat(mView.getTitleText().getText().toString(), equalTo(testTitle));
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
