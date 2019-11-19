// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.app.Activity;
import android.graphics.Color;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.AppCompatEditText;
import android.text.InputType;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.night_mode.NightModeTestUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.LocalizationUtils;

import java.io.IOException;
import java.util.List;

/**
 * Render tests for the ChromeTextInputLayout
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class ChromeTextInputLayoutRenderTest extends DummyUiActivityTestCase {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public RenderTestRule mRenderTestRule = new RenderTestRule();

    private static final String LABEL = "Label";
    private static final String TEXT = "Chrome's own TextInputLayout";
    private static final String PHONE = "+1 555-555-5555";
    private static final String ERROR = "Error";

    private final int mFakeBgColor;

    private ChromeTextInputLayout mInputLayout;
    private AppCompatEditText mEditText;

    public ChromeTextInputLayoutRenderTest(boolean nightModeEnabled) {
        // Sets a fake background color to make the screenshots easier to compare with bare eyes.
        mFakeBgColor = nightModeEnabled ? Color.BLACK : Color.WHITE;
        NightModeTestUtils.setUpNightModeForDummyUiActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        final Activity activity = getActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mInputLayout = new ChromeTextInputLayout(
                    new ContextThemeWrapper(activity, R.style.Theme_Chromium_Preferences));
            ViewGroup.LayoutParams layoutParams = new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            mInputLayout.setLayoutParams(layoutParams);
            mInputLayout.setBackgroundColor(mFakeBgColor);
            ((ViewGroup) activity.findViewById(android.R.id.content)).addView(mInputLayout);
            mEditText = new AppCompatEditText(
                    new ContextThemeWrapper(activity, R.style.Theme_Chromium_Preferences));
            mInputLayout.addView(mEditText);
            mInputLayout.setHint(LABEL);
            RenderTestRule.sanitize(mEditText);
        });
    }

    @Override
    public void tearDownTest() throws Exception {
        NightModeTestUtils.tearDownNightModeForDummyUiActivity();
        super.tearDownTest();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testExpanded() throws IOException {
        mRenderTestRule.render(mInputLayout, "expanded");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testFocused() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> mEditText.requestFocus());
        mRenderTestRule.render(mInputLayout, "focused");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testFilled() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> mEditText.setText(TEXT));
        mRenderTestRule.render(mInputLayout, "filled");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testError() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> mInputLayout.setError(ERROR));
        mRenderTestRule.render(mInputLayout, "error");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testFocusedRTL() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            setLayoutDirectionRTL();
            mEditText.requestFocus();
        });
        mRenderTestRule.render(mInputLayout, "focused_rtl");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testExpandedRTL() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(this::setLayoutDirectionRTL);
        mRenderTestRule.render(mInputLayout, "expanded_rtl");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPhoneRTL() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            setLayoutDirectionRTL();
            mEditText.setInputType(InputType.TYPE_CLASS_PHONE);
        });
        mRenderTestRule.render(mInputLayout, "phone_expanded_rtl");
        TestThreadUtils.runOnUiThreadBlocking(() -> mEditText.setText(PHONE));
        mRenderTestRule.render(mInputLayout, "phone_rtl");
    }

    private void setLayoutDirectionRTL() {
        LocalizationUtils.setRtlForTesting(true);
        mInputLayout.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mInputLayout.setHint(LABEL);
    }
}
