// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.support.test.filters.MediumTest;
import android.text.TextUtils;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.ColorInt;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.night_mode.NightModeTestUtils;
import org.chromium.chrome.browser.night_mode.NightModeTestUtils.NightModeParams;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.io.IOException;
import java.util.Collections;
import java.util.List;

/**
 * Render tests for {@link ModalDialogView}.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class ModalDialogViewRenderTest extends DummyUiActivityTestCase {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams = new NightModeParams().getParameters();

    private final @ColorInt int mFakeBgColor;

    private Resources mResources;
    private PropertyModel.Builder mModelBuilder;
    private FrameLayout mContentView;
    private ModalDialogView mModalDialogView;
    private ScrollView mCustomScrollView;
    private TextView mCustomTextView1;
    private TextView mCustomTextView2;

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("chrome/test/data/android/render_tests");

    public ModalDialogViewRenderTest(boolean nightModeEnabled) {
        // Sets a fake background color to make the screenshots easier to compare with bare eyes.
        mFakeBgColor = nightModeEnabled ? Color.BLACK : Color.WHITE;
        NightModeTestUtils.setUpNightModeForDummyUiActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
    }

    @Override
    public void tearDownTest() throws Exception {
        NightModeTestUtils.tearDownNightModeForDummyUiActivity();
        super.tearDownTest();
    }

    private void setUpViews(int style) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Activity activity = getActivity();
            mResources = activity.getResources();
            mModelBuilder = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS);

            mContentView = new FrameLayout(activity);
            mModalDialogView =
                    (ModalDialogView) LayoutInflater.from(new ContextThemeWrapper(activity, style))
                            .inflate(org.chromium.chrome.R.layout.modal_dialog_view, null);
            mModalDialogView.setBackgroundColor(mFakeBgColor);
            activity.setContentView(mContentView);
            mContentView.addView(mModalDialogView, MATCH_PARENT, WRAP_CONTENT);

            mCustomScrollView = new ScrollView(activity);
            mCustomTextView1 = new TextView(activity);
            mCustomTextView1.setId(R.id.test_view_one);
            mCustomTextView2 = new TextView(activity);
            mCustomTextView2.setId(R.id.test_view_two);
        });
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_TitleAndTitleIcon() throws IOException {
        setUpViews(org.chromium.chrome.R.style.Theme_Chromium_ModalDialog_TextPrimaryButton);
        final Drawable icon =
                UiUtils.getTintedDrawable(getActivity(), org.chromium.chrome.R.drawable.ic_add,
                        org.chromium.chrome.R.color.default_icon_color);
        createModel(mModelBuilder
                            .with(ModalDialogProperties.TITLE, mResources,
                                    org.chromium.chrome.R.string.title)
                            .with(ModalDialogProperties.TITLE_ICON, icon));
        mRenderTestRule.render(mModalDialogView, "title_and_title_icon");
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_TitleAndMessage() throws IOException {
        setUpViews(org.chromium.chrome.R.style.Theme_Chromium_ModalDialog_TextPrimaryButton);
        createModel(mModelBuilder
                            .with(ModalDialogProperties.TITLE, mResources,
                                    org.chromium.chrome.R.string.title)
                            .with(ModalDialogProperties.MESSAGE,
                                    TextUtils.join("\n", Collections.nCopies(100, "Message")))
                            .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mResources,
                                    org.chromium.chrome.R.string.ok)
                            .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true)
                            .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, mResources,
                                    org.chromium.chrome.R.string.cancel));
        mRenderTestRule.render(mModalDialogView, "title_and_message");
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_FilledPrimaryButton() throws IOException {
        setUpViews(org.chromium.chrome.R.style.Theme_Chromium_ModalDialog_FilledPrimaryButton);
        createModel(mModelBuilder
                            .with(ModalDialogProperties.TITLE, mResources,
                                    org.chromium.chrome.R.string.title)
                            .with(ModalDialogProperties.MESSAGE,
                                    TextUtils.join("\n", Collections.nCopies(100, "Message")))
                            .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mResources,
                                    org.chromium.chrome.R.string.ok)
                            .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true)
                            .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, mResources,
                                    org.chromium.chrome.R.string.cancel));
        mRenderTestRule.render(mModalDialogView, "filled_primary_button");
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_ScrollableTitle() throws IOException {
        setUpViews(org.chromium.chrome.R.style.Theme_Chromium_ModalDialog_TextPrimaryButton);
        createModel(mModelBuilder
                            .with(ModalDialogProperties.TITLE, mResources,
                                    org.chromium.chrome.R.string.title)
                            .with(ModalDialogProperties.TITLE_SCROLLABLE, true)
                            .with(ModalDialogProperties.MESSAGE,
                                    TextUtils.join("\n", Collections.nCopies(100, "Message")))
                            .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mResources,
                                    org.chromium.chrome.R.string.ok));
        mRenderTestRule.render(mModalDialogView, "scrollable_title");
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_CustomView() throws IOException {
        setUpViews(org.chromium.chrome.R.style.Theme_Chromium_ModalDialog_TextPrimaryButton);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCustomTextView1.setText(
                    TextUtils.join("\n", Collections.nCopies(100, "Custom Message")));
            mCustomScrollView.addView(mCustomTextView1);
        });
        createModel(mModelBuilder
                            .with(ModalDialogProperties.TITLE, mResources,
                                    org.chromium.chrome.R.string.title)
                            .with(ModalDialogProperties.CUSTOM_VIEW, mCustomScrollView)
                            .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mResources,
                                    org.chromium.chrome.R.string.ok));
        mRenderTestRule.render(mModalDialogView, "custom_view");
    }

    private PropertyModel createModel(PropertyModel.Builder modelBuilder) {
        return ModalDialogTestUtils.createModel(modelBuilder, mModalDialogView);
    }
}
