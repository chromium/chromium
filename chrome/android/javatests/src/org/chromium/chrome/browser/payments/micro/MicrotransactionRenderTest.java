// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.micro;

import android.support.annotation.Nullable;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for the microtransaction view binder. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class MicrotransactionRenderTest extends DummyUiActivityTestCase {
    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("components/test/data/payments/render_tests");

    private PropertyModel mModel;
    private MicrotransactionView mView;
    private RelativeLayout mLayout;
    private PropertyModelChangeProcessor mProcessor;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        // Initial button state:
        mModel =
                new PropertyModel.Builder(MicrotransactionProperties.ALL_KEYS)
                        .with(MicrotransactionProperties.ACCOUNT_BALANCE, "$18.00")
                        .with(MicrotransactionProperties.AMOUNT, "$1.00")
                        .with(MicrotransactionProperties.CURRENCY, "USD")
                        .with(MicrotransactionProperties.IS_SHOWING_LINE_ITEMS, true)
                        .with(MicrotransactionProperties.IS_SHOWING_PAY_BUTTON, true)
                        .with(MicrotransactionProperties.IS_SHOWING_PROCESSING_SPINNER, false)
                        .with(MicrotransactionProperties.PAYMENT_APP_ICON,
                                VectorDrawableCompat.create(getActivity().getResources(),
                                        R.drawable.ic_done_googblue_36dp, getActivity().getTheme()))
                        .with(MicrotransactionProperties.PAYMENT_APP_NAME, "App Name")
                        .with(MicrotransactionProperties.STATUS_TEXT_RESOURCE,
                                R.string.payment_request_payment_method_section_name)
                        .build();

        mView = new MicrotransactionView(getActivity());
        mProcessor = PropertyModelChangeProcessor.create(
                mModel, mView, MicrotransactionViewBinder::bind);

        mLayout = new RelativeLayout(getActivity());
        RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        mLayout.addView(mView.getContentView(), params);
        mLayout.addView(mView.getToolbarView(), params);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().setContentView(mLayout,
                    new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT));
        });
    }

    @Override
    public void tearDownTest() {
        mProcessor.destroy();
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testButtonInitial() throws Throwable {
        mRenderTestRule.render(mLayout, "button_initial");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testButtonInitialExpanded() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA, 1f));
        mRenderTestRule.render(mLayout, "button_initial_expanded");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testButtonProcessing() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> showButtonProcessing());
        mRenderTestRule.render(mLayout, "button_processing");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testButtonProcessingExpanded() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA, 1f);
            showButtonProcessing();
        });
        mRenderTestRule.render(mLayout, "button_processing_expanded");
    }

    private void showButtonProcessing() {
        mModel.set(MicrotransactionProperties.IS_SHOWING_LINE_ITEMS, false);
        mModel.set(MicrotransactionProperties.IS_SHOWING_PAY_BUTTON, false);
        mModel.set(MicrotransactionProperties.IS_SHOWING_PROCESSING_SPINNER, true);
        mModel.set(MicrotransactionProperties.STATUS_TEXT, null);
        mModel.set(MicrotransactionProperties.STATUS_TEXT_RESOURCE,
                R.string.payments_processing_message);

        stopSpinner(mView.mToolbarProcessingSpinner);
        stopSpinner(mView.mContentProcessingSpinner);
    }

    private void stopSpinner(View view) {
        ProgressBar spinner = (ProgressBar) view;
        spinner.setIndeterminate(false);
        spinner.setMax(2);
        spinner.setProgress(1);
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testButtonErrorResource() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            showEmphasizedStatus(R.string.payment_fingerprint_not_recognized, null,
                    R.drawable.ic_error_googred_36dp, R.color.microtransaction_error_tint);
            mModel.set(MicrotransactionProperties.IS_SHOWING_LINE_ITEMS, false);
        });
        mRenderTestRule.render(mLayout, "button_error_resource");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testButtonErrorResourceExpanded() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA, 1f);
            showEmphasizedStatus(R.string.payment_fingerprint_not_recognized, null,
                    R.drawable.ic_error_googred_36dp, R.color.microtransaction_error_tint);
            mModel.set(MicrotransactionProperties.IS_SHOWING_LINE_ITEMS, false);
        });
        mRenderTestRule.render(mLayout, "button_error_resource_expanded");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testButtonErrorString() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            showEmphasizedStatus(null, "Finger moved too fast. Try again.",
                    R.drawable.ic_error_googred_36dp, R.color.microtransaction_error_tint);
            mModel.set(MicrotransactionProperties.IS_SHOWING_LINE_ITEMS, false);
        });
        mRenderTestRule.render(mLayout, "button_error_string");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testButtonErrorStringExpanded() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA, 1f);
            showEmphasizedStatus(null, "Finger moved too fast. Try again.",
                    R.drawable.ic_error_googred_36dp, R.color.microtransaction_error_tint);
            mModel.set(MicrotransactionProperties.IS_SHOWING_LINE_ITEMS, false);
        });
        mRenderTestRule.render(mLayout, "button_error_string_expanded");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testButtonSuccess() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            showEmphasizedStatus(R.string.payment_complete_message, null,
                    R.drawable.ic_done_googblue_36dp, R.color.microtransaction_emphasis_tint);
            mModel.set(MicrotransactionProperties.IS_SHOWING_LINE_ITEMS, false);
        });
        mRenderTestRule.render(mLayout, "button_success");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testButtonSuccessExpanded() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA, 1f);
            showEmphasizedStatus(R.string.payment_complete_message, null,
                    R.drawable.ic_done_googblue_36dp, R.color.microtransaction_emphasis_tint);
            mModel.set(MicrotransactionProperties.IS_SHOWING_LINE_ITEMS, false);
        });
        mRenderTestRule.render(mLayout, "button_success_expanded");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testFingerprintInitial() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> showFingerprintInitial());
        mRenderTestRule.render(mLayout, "fingerprint_initial");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testFingerprintInitialExpanded() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA, 1f);
            showFingerprintInitial();
        });
        mRenderTestRule.render(mLayout, "fingerprint_initial_expanded");
    }

    private void showFingerprintInitial() {
        mModel.set(MicrotransactionProperties.IS_SHOWING_PAY_BUTTON, false);
        mModel.set(MicrotransactionProperties.STATUS_ICON, R.drawable.ic_fingerprint_grey500_36dp);
        mModel.set(
                MicrotransactionProperties.STATUS_ICON_TINT, R.color.microtransaction_default_tint);
        mModel.set(MicrotransactionProperties.STATUS_TEXT_RESOURCE,
                R.string.payment_touch_sensor_to_pay);
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testFingerprintProcessing() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> showFingerprintProcessing());
        mRenderTestRule.render(mLayout, "fingerprint_processing");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testFingerprintProcessingExpanded() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA, 1f);
            showFingerprintProcessing();
        });
        mRenderTestRule.render(mLayout, "fingerprint_processing_expanded");
    }

    private void showFingerprintProcessing() {
        mModel.set(MicrotransactionProperties.IS_SHOWING_PAY_BUTTON, false);
        mModel.set(MicrotransactionProperties.STATUS_TEXT, null);
        mModel.set(MicrotransactionProperties.STATUS_TEXT_RESOURCE,
                R.string.payments_processing_message);
        mModel.set(MicrotransactionProperties.STATUS_ICON, R.drawable.ic_fingerprint_grey500_36dp);
        mModel.set(MicrotransactionProperties.STATUS_ICON_TINT,
                R.color.microtransaction_emphasis_tint);
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testFingerprintErrorResource() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            showEmphasizedStatus(R.string.payment_fingerprint_not_recognized, null,
                    R.drawable.ic_error_googred_36dp, R.color.microtransaction_error_tint);
        });
        mRenderTestRule.render(mLayout, "fingerprint_error_resource");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testFingerprintErrorResourceExpanded() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA, 1f);
            showEmphasizedStatus(R.string.payment_fingerprint_not_recognized, null,
                    R.drawable.ic_error_googred_36dp, R.color.microtransaction_error_tint);
        });
        mRenderTestRule.render(mLayout, "fingerprint_error_resource_expanded");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testFingerprintErrorString() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            showEmphasizedStatus(null, "Finger moved too fast. Try again.",
                    R.drawable.ic_error_googred_36dp, R.color.microtransaction_error_tint);
        });
        mRenderTestRule.render(mLayout, "fingerprint_error_string");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testFingerprintErrorStringExpanded() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA, 1f);
            showEmphasizedStatus(null, "Finger moved too fast. Try again.",
                    R.drawable.ic_error_googred_36dp, R.color.microtransaction_error_tint);
        });
        mRenderTestRule.render(mLayout, "fingerprint_error_string_expanded");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testFingerprintSuccess() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            showEmphasizedStatus(R.string.payment_complete_message, null,
                    R.drawable.ic_done_googblue_36dp, R.color.microtransaction_emphasis_tint);
        });
        mRenderTestRule.render(mLayout, "fingerprint_success");
    }

    @Test
    @SmallTest
    @Feature({"Microtransaction", "RenderTest"})
    public void testFingerprintSuccessExpanded() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA, 1f);
            showEmphasizedStatus(R.string.payment_complete_message, null,
                    R.drawable.ic_done_googblue_36dp, R.color.microtransaction_emphasis_tint);
        });
        mRenderTestRule.render(mLayout, "fingerprint_success_expanded");
    }

    private void showEmphasizedStatus(@Nullable Integer messageResourceId,
            @Nullable CharSequence message, @Nullable Integer iconResourceId,
            @Nullable Integer iconTint) {
        mModel.set(MicrotransactionProperties.IS_SHOWING_PAY_BUTTON, false);
        mModel.set(MicrotransactionProperties.STATUS_TEXT, message);
        mModel.set(MicrotransactionProperties.STATUS_TEXT_RESOURCE, messageResourceId);
        mModel.set(MicrotransactionProperties.IS_STATUS_EMPHASIZED, true);
        mModel.set(MicrotransactionProperties.STATUS_ICON, iconResourceId);
        mModel.set(MicrotransactionProperties.STATUS_ICON_TINT, iconTint);
        mModel.set(MicrotransactionProperties.IS_SHOWING_PROCESSING_SPINNER, false);
    }
}
