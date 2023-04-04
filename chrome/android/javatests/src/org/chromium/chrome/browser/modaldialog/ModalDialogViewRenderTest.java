// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.Collections;
import java.util.List;

/**
 * Render tests for {@link ModalDialogView}.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class ModalDialogViewRenderTest extends BlankUiTestActivityTestCase {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    private final @ColorInt int mFakeBgColor;

    private Resources mResources;
    private PropertyModel.Builder mModelBuilder;
    private FrameLayout mContentView;
    private ModalDialogView mModalDialogView;
    private ScrollView mCustomScrollView;
    private TextView mCustomTextView1;
    private TextView mCustomTextView2;
    private RelativeLayout mCustomButtonBar;

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_MESSAGES)
                    .build();

    public ModalDialogViewRenderTest(boolean nightModeEnabled) {
        // Sets a fake background color to make the screenshots easier to compare with bare eyes.
        mFakeBgColor = nightModeEnabled ? Color.BLACK : Color.WHITE;
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Override
    public void tearDownTest() throws Exception {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
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
                            .inflate(R.layout.modal_dialog_view, null);
            mModalDialogView.setBackgroundColor(mFakeBgColor);
            activity.setContentView(mContentView);
            mContentView.addView(mModalDialogView, MATCH_PARENT, WRAP_CONTENT);

            mCustomScrollView = new ScrollView(activity);
            mCustomTextView1 = new TextView(activity);
            mCustomTextView1.setId(R.id.test_view_one);
            mCustomTextView2 = new TextView(activity);
            mCustomTextView2.setId(R.id.test_view_two);

            mCustomButtonBar = new RelativeLayout(activity);
            mCustomButtonBar.setId(R.id.test_button_bar_one);
            Button button = new Button(activity);
            button.setText(R.string.ok);
            RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            params.addRule(RelativeLayout.ALIGN_PARENT_LEFT, RelativeLayout.TRUE);
            mCustomButtonBar.addView(button, params);
        });
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_TitleAndTitleIcon() throws IOException {
        setUpViews(R.style.ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton);
        final Drawable icon = UiUtils.getTintedDrawable(
                getActivity(), R.drawable.ic_add, R.color.default_icon_color_tint_list);
        createModel(mModelBuilder.with(ModalDialogProperties.TITLE, mResources, R.string.title)
                            .with(ModalDialogProperties.TITLE_ICON, icon));
        mRenderTestRule.render(mModalDialogView, "title_and_title_icon");
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_TitleAndMessage() throws IOException {
        setUpViews(R.style.ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton);
        createModel(
                mModelBuilder.with(ModalDialogProperties.TITLE, mResources, R.string.title)
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                TextUtils.join("\n", Collections.nCopies(100, "Message")))
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mResources, R.string.ok)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, mResources,
                                R.string.cancel));
        mRenderTestRule.render(mModalDialogView, "title_and_message");
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_FilledPrimaryButton() throws IOException {
        setUpViews(R.style.ThemeOverlay_BrowserUI_ModalDialog_FilledPrimaryButton);
        createModel(
                mModelBuilder.with(ModalDialogProperties.TITLE, mResources, R.string.title)
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                TextUtils.join("\n", Collections.nCopies(100, "Message")))
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mResources, R.string.ok)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, mResources,
                                R.string.cancel));
        mRenderTestRule.render(mModalDialogView, "filled_primary_button");
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_ScrollableTitle() throws IOException {
        setUpViews(R.style.ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton);
        createModel(
                mModelBuilder.with(ModalDialogProperties.TITLE, mResources, R.string.title)
                        .with(ModalDialogProperties.TITLE_SCROLLABLE, true)
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                TextUtils.join("\n", Collections.nCopies(100, "Message")))
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mResources, R.string.ok));
        mRenderTestRule.render(mModalDialogView, "scrollable_title");
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_CustomView() throws IOException {
        setUpViews(R.style.ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton);
        var sb = new StringBuilder();
        for (int i = 0; i < 100; i++) {
            sb.append(i).append("\n");
        }
        sb.append(100);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCustomTextView1.setText(sb.toString());
            mCustomScrollView.addView(mCustomTextView1);
        });
        createModel(
                mModelBuilder.with(ModalDialogProperties.TITLE, mResources, R.string.title)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mCustomScrollView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mResources, R.string.ok));
        mRenderTestRule.render(mModalDialogView, "custom_view");
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_CustomButtonBarView() throws IOException {
        setUpViews(R.style.ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton);
        createModel(
                mModelBuilder.with(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW, mCustomButtonBar)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mResources, R.string.ok));
        mRenderTestRule.render(mModalDialogView, "custom_button_bar_view");
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_FooterMessage() throws IOException {
        setUpViews(R.style.ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton);
        createModel(
                mModelBuilder.with(ModalDialogProperties.TITLE, mResources, R.string.title)
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                TextUtils.join("\n", Collections.nCopies(100, "Message")))
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mResources, R.string.ok)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, mResources,
                                R.string.cancel)
                        .with(ModalDialogProperties.FOOTER_MESSAGE,
                                mResources.getString(R.string.legal_information_summary)));
        mRenderTestRule.render(mModalDialogView, "footer_message");
    }

    private PropertyModel createModel(PropertyModel.Builder modelBuilder) {
        return ModalDialogTestUtils.createModel(modelBuilder, mModalDialogView);
    }
}
