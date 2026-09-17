// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule;

/** Render tests for {@link TabBottomSheetPeekView}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TabBottomSheetPeekViewRenderTest {
    private static final String DEFAULT_TITLE = "Gemini in Chrome";
    private static final String ACTOR_TITLE = "Shopping Assistant";
    private static final int ACTOR_TITLE_APPEARANCE =
            R.style.TextAppearance_TextMediumThick_Primary;
    private static final int ACTION_BUTTON_TINT = R.color.default_bg_color_blue;

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_GLIC)
                    .setRevision(0)
                    .setDescription("Initial render tests for TabBottomSheetPeekView")
                    .build();

    private Activity mActivity;
    private FrameLayout mParentView;
    private TabBottomSheetPeekView mPeekView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mChangeProcessor;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();

        runOnUiThreadBlocking(
                () -> {
                    mParentView = new FrameLayout(mActivity);
                    mActivity.setContentView(mParentView);

                    mPeekView =
                            (TabBottomSheetPeekView)
                                    LayoutInflater.from(mActivity)
                                            .inflate(
                                                    R.layout.tab_bottom_sheet_peek_layout,
                                                    mParentView,
                                                    false);
                    FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                            FrameLayout.LayoutParams.MATCH_PARENT,
                            mActivity.getResources().getDimensionPixelSize(R.dimen.tab_bottom_sheet_peek_height_total));
                    mParentView.addView(mPeekView, params);

                    mModel =
                            new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS)
                                    .build();
                    mChangeProcessor =
                            PropertyModelChangeProcessor.create(
                                    mModel, mPeekView, TabBottomSheetPeekViewBinder::bind);
                });
    }

    @After
    public void tearDown() {
        runOnUiThreadBlocking(
                () -> {
                    mChangeProcessor.destroy();
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_DefaultState() throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(TabBottomSheetPeekProperties.TITLE_TEXT, DEFAULT_TITLE);
                    mModel.set(
                            TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE_ID,
                            R.style.TextAppearance_Headline2Thick);
                    mModel.set(TabBottomSheetPeekProperties.DESCRIPTION_VISIBILITY, View.GONE);
                    mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY, View.GONE);
                });
        mRenderTestRule.render(mPeekView, "tab_bottom_sheet_peek_default");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_ActingState() throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(TabBottomSheetPeekProperties.TITLE_TEXT, ACTOR_TITLE);
                    mModel.set(
                            TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE_ID,
                            ACTOR_TITLE_APPEARANCE);
                    mModel.set(
                            TabBottomSheetPeekProperties.DESCRIPTION_TEXT_ID,
                            R.string.peek_state_acting);
                    mModel.set(TabBottomSheetPeekProperties.DESCRIPTION_VISIBILITY, View.VISIBLE);
                    mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY, View.VISIBLE);
                    mModel.set(
                            TabBottomSheetPeekProperties.ACTION_BUTTON_ICON_ID,
                            R.drawable.ic_pause_white_24dp);
                    mModel.set(
                            TabBottomSheetPeekProperties.ACTION_BUTTON_BACKGROUND_TINT_ID,
                            ACTION_BUTTON_TINT);
                });
        mRenderTestRule.render(mPeekView, "tab_bottom_sheet_peek_acting");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_PausedState() throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(TabBottomSheetPeekProperties.TITLE_TEXT, ACTOR_TITLE);
                    mModel.set(
                            TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE_ID,
                            ACTOR_TITLE_APPEARANCE);
                    mModel.set(
                            TabBottomSheetPeekProperties.DESCRIPTION_TEXT_ID,
                            R.string.peek_state_paused);
                    mModel.set(TabBottomSheetPeekProperties.DESCRIPTION_VISIBILITY, View.VISIBLE);
                    mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY, View.VISIBLE);
                    mModel.set(
                            TabBottomSheetPeekProperties.ACTION_BUTTON_ICON_ID,
                            R.drawable.ic_play_arrow_white_24dp);
                    mModel.set(
                            TabBottomSheetPeekProperties.ACTION_BUTTON_BACKGROUND_TINT_ID,
                            ACTION_BUTTON_TINT);
                });
        mRenderTestRule.render(mPeekView, "tab_bottom_sheet_peek_paused");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_WaitingState() throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(TabBottomSheetPeekProperties.TITLE_TEXT, ACTOR_TITLE);
                    mModel.set(
                            TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE_ID,
                            ACTOR_TITLE_APPEARANCE);
                    mModel.set(
                            TabBottomSheetPeekProperties.DESCRIPTION_TEXT_ID,
                            R.string.peek_state_waiting);
                    mModel.set(TabBottomSheetPeekProperties.DESCRIPTION_VISIBILITY, View.VISIBLE);
                    mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY, View.VISIBLE);
                    mModel.set(
                            TabBottomSheetPeekProperties.ACTION_BUTTON_TEXT_ID,
                            R.string.peek_state_view_button_label);
                    mModel.set(
                            TabBottomSheetPeekProperties.ACTION_BUTTON_BACKGROUND_TINT_ID,
                            ACTION_BUTTON_TINT);
                });
        mRenderTestRule.render(mPeekView, "tab_bottom_sheet_peek_waiting");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_ConversationTitle() throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(TabBottomSheetPeekProperties.TITLE_TEXT, "How to bake a cake");
                    mModel.set(
                            TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE_ID,
                            R.style.TextAppearance_Headline2Thick);
                    mModel.set(TabBottomSheetPeekProperties.DESCRIPTION_VISIBILITY, View.GONE);
                    mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY, View.GONE);
                });
        mRenderTestRule.render(mPeekView, "tab_bottom_sheet_peek_conversation_title");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_NewChatFallback() throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(
                            TabBottomSheetPeekProperties.TITLE_TEXT,
                            mActivity.getString(R.string.peek_state_new_chat));
                    mModel.set(
                            TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE_ID,
                            R.style.TextAppearance_Headline2Thick);
                    mModel.set(TabBottomSheetPeekProperties.DESCRIPTION_VISIBILITY, View.GONE);
                    mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY, View.GONE);
                });
        mRenderTestRule.render(mPeekView, "tab_bottom_sheet_peek_new_chat_fallback");
    }
}
