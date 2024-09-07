// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.app.Activity;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.ui.test.util.BlankUiTestActivity;

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TabCardLabelViewTest {

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private TabCardLabelView mTabCardLabelView;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(/* startIntent= */ null);
        Activity activity = mActivityTestRule.getActivity();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        runOnUiThreadBlocking(
                () -> {
                    FrameLayout contentView = new FrameLayout(activity);
                    contentView.setBackgroundColor(Color.WHITE);

                    mTabCardLabelView =
                            (TabCardLabelView)
                                    LayoutInflater.from(activity)
                                            .inflate(R.layout.tab_card_label_layout, null);
                    LayoutParams params =
                            new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
                    contentView.addView(mTabCardLabelView, params);

                    params = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
                    activity.setContentView(contentView, params);
                });
    }

    @Test
    @SmallTest
    public void testContentDescription() {
        String contentDescription = "Bob changed";
        TextResolver textResolver = buildTextResolver("Alice changed");
        TextResolver contentDescriptionResolver = buildTextResolver(contentDescription);
        TabCardLabelData contentDescriptionData =
                new TabCardLabelData(
                        TabCardLabelType.ACTIVITY_UPDATE,
                        textResolver,
                        /* asyncImageFactory= */ null,
                        contentDescriptionResolver);
        runOnUiThreadBlocking(
                () -> {
                    mTabCardLabelView.setData(contentDescriptionData);
                    assertEquals(contentDescription, mTabCardLabelView.getContentDescription());
                    assertEquals(
                            View.IMPORTANT_FOR_ACCESSIBILITY_NO,
                            mTabCardLabelView
                                    .findViewById(R.id.tab_label_text)
                                    .getImportantForAccessibility());
                });

        TabCardLabelData noContentDescriptionData =
                new TabCardLabelData(
                        TabCardLabelType.ACTIVITY_UPDATE,
                        textResolver,
                        /* asyncImageFactory= */ null,
                        /* contentDescriptionResolver= */ null);
        runOnUiThreadBlocking(
                () -> {
                    mTabCardLabelView.setData(noContentDescriptionData);
                    assertNull(mTabCardLabelView.getContentDescription());
                    assertEquals(
                            View.IMPORTANT_FOR_ACCESSIBILITY_AUTO,
                            mTabCardLabelView
                                    .findViewById(R.id.tab_label_text)
                                    .getImportantForAccessibility());
                });
    }

    private TextResolver buildTextResolver(String string) {
        return (context) -> {
            return string;
        };
    }
}
