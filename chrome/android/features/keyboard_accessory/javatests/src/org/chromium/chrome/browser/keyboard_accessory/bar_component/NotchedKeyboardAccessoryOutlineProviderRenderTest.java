// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Path;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryStyle.NotchPosition;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Render tests for {@link NotchedKeyboardAccessoryOutlineProvider}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class NotchedKeyboardAccessoryOutlineProviderRenderTest {

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_AUTOFILL)
                    .build();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private FrameLayout mContentView;
    private View mView;
    private int mNotchOffsetX;

    private static final int VIEW_WIDTH = 300;
    private static final int VIEW_HEIGHT = 100;
    private static final int MARGINS = 100;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContentView = new FrameLayout(mActivityTestRule.getActivity());
                    mContentView.setBackgroundColor(Color.WHITE);

                    // Custom view to handle software rendering used by render tests. Clip path
                    // calculated by the outline logic needs to be applied on the canvas to be
                    // visible on the screenshots, because hardware accelerated outlining is not
                    // supported in this environment.
                    mView =
                            new View(mActivityTestRule.getActivity()) {
                                @Override
                                public void draw(Canvas canvas) {
                                    // Retrieve the notch position from the tag.
                                    Object tag = getTag();
                                    if (tag instanceof Integer) {
                                        int position = (Integer) tag;

                                        // Now receives the Path directly from the helper method
                                        Path clipPath =
                                                NotchedKeyboardAccessoryOutlineProvider
                                                        .createNotchPath(
                                                                getResources(),
                                                                position,
                                                                getWidth(),
                                                                getHeight(),
                                                                mNotchOffsetX);

                                        canvas.clipPath(clipPath);
                                    }
                                    super.draw(canvas);
                                }
                            };

                    mView.setBackgroundColor(Color.BLUE);

                    // Keep this enabled. Even though the software renderer ignores it to ensure
                    // the real production pipeline is configured correctly.
                    mView.setClipToOutline(true);

                    FrameLayout.LayoutParams layoutParams =
                            new FrameLayout.LayoutParams(VIEW_WIDTH, VIEW_HEIGHT);
                    layoutParams.setMargins(MARGINS, MARGINS, MARGINS, MARGINS);
                    mContentView.addView(mView, layoutParams);

                    mActivityTestRule.getActivity().setContentView(mContentView);
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderWithTopNotchAndOffset() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNotchOffsetX = 50;
                    mView.setTag(NotchPosition.TOP);
                    NotchedKeyboardAccessoryOutlineProvider provider =
                            new NotchedKeyboardAccessoryOutlineProvider(NotchPosition.TOP);
                    provider.setNotchOffsetX(mNotchOffsetX);
                    mView.setOutlineProvider(provider);
                });
        CriteriaHelper.pollUiThread(() -> mView.getHeight() > 0 && mView.getWidth() > 0);
        mRenderTestRule.render(mView, "top_notch_offset");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderWithTopNotch() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setTag(NotchPosition.TOP);
                    mView.setOutlineProvider(
                            new NotchedKeyboardAccessoryOutlineProvider(NotchPosition.TOP));
                });
        CriteriaHelper.pollUiThread(() -> mView.getHeight() > 0 && mView.getWidth() > 0);
        mRenderTestRule.render(mView, "top_notch");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderWithBottomNotch() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setTag(NotchPosition.BOTTOM);
                    mView.setOutlineProvider(
                            new NotchedKeyboardAccessoryOutlineProvider(NotchPosition.BOTTOM));
                });
        CriteriaHelper.pollUiThread(() -> mView.getHeight() > 0 && mView.getWidth() > 0);
        mRenderTestRule.render(mView, "bottom_notch");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderWithHiddenNotch() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView.setTag(NotchPosition.HIDDEN);
                    mView.setOutlineProvider(
                            new NotchedKeyboardAccessoryOutlineProvider(NotchPosition.HIDDEN));
                });
        CriteriaHelper.pollUiThread(() -> mView.getHeight() > 0 && mView.getWidth() > 0);
        mRenderTestRule.render(mView, "hidden_notch");
    }
}
