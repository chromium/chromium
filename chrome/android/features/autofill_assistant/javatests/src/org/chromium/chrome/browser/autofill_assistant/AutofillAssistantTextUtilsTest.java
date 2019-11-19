// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.assertThat;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.openTextLink;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.graphics.Typeface;
import android.support.test.filters.MediumTest;
import android.text.SpannedString;
import android.text.style.StyleSpan;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Tests for {@code AssistantTextUtils}. */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantTextUtilsTest {
    private LinearLayout mTestLayout;

    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Before
    public void setUp() {
        AutofillAssistantUiTestUtil.startOnBlankPage(mTestRule);
        runOnUiThreadBlocking(() -> {
            mTestLayout = new LinearLayout(mTestRule.getActivity());
            mTestLayout.setOrientation(LinearLayout.VERTICAL);
            AutofillAssistantUiTestUtil.attachToCoordinator(mTestRule.getActivity(), mTestLayout);
        });
    }

    @Test
    @MediumTest
    public void testTextStyles() throws Exception {
        TextView regularTextView = runOnUiThreadBlocking(() -> {
            TextView view = new TextView(mTestRule.getActivity());
            mTestLayout.addView(view);
            AssistantTextUtils.applyVisualAppearanceTags(view, "Regular text", null);
            StyleSpan[] spans =
                    ((SpannedString) view.getText()).getSpans(0, view.length(), StyleSpan.class);
            assertThat(spans.length, is(0));
            return view;
        });
        onView(is(regularTextView)).check(matches(withText("Regular text")));

        TextView boldTextView = runOnUiThreadBlocking(() -> {
            TextView view = new TextView(mTestRule.getActivity());
            mTestLayout.addView(view);
            AssistantTextUtils.applyVisualAppearanceTags(view, "<b>Bold text</b>", null);
            StyleSpan[] spans =
                    ((SpannedString) view.getText()).getSpans(0, view.length(), StyleSpan.class);
            assertThat(spans.length, is(1));
            assertThat(spans[0].getStyle(), is(Typeface.BOLD));
            return view;
        });
        onView(is(boldTextView)).check(matches(withText("Bold text")));

        TextView italicTextView = runOnUiThreadBlocking(() -> {
            TextView view = new TextView(mTestRule.getActivity());
            mTestLayout.addView(view);
            AssistantTextUtils.applyVisualAppearanceTags(view, "<i>Italic text</i>", null);
            StyleSpan[] spans =
                    ((SpannedString) view.getText()).getSpans(0, view.length(), StyleSpan.class);
            assertThat(spans.length, is(1));
            assertThat(spans[0].getStyle(), is(Typeface.ITALIC));
            return view;
        });
        onView(is(italicTextView)).check(matches(withText("Italic text")));
    }

    @Test
    @MediumTest
    public void testTextLinks() throws Exception {
        /* Simple helper class that keeps track of the most recent callback result. */
        class LinkCallback implements Callback<Integer> {
            private int mLastCallback = -1;

            private int getLastCallback() {
                return mLastCallback;
            }
            @Override
            public void onResult(Integer result) {
                mLastCallback = result;
            }
        }

        LinkCallback linkCallback = new LinkCallback();
        TextView multiLinkView = runOnUiThreadBlocking(() -> {
            TextView view = new TextView(mTestRule.getActivity());
            mTestLayout.addView(view);
            AssistantTextUtils.applyVisualAppearanceTags(view,
                    "Click <link1>here</link1> or <link2>there</link2> or "
                            + "<link3>somewhere else</link3>.",
                    linkCallback);
            return view;
        });
        onView(is(multiLinkView))
                .check(matches(withText("Click here or there or somewhere else.")));
        onView(is(multiLinkView)).perform(openTextLink("here"));
        assertThat(linkCallback.getLastCallback(), is(1));
        onView(is(multiLinkView)).perform(openTextLink("somewhere else"));
        assertThat(linkCallback.getLastCallback(), is(3));
        onView(is(multiLinkView)).perform(openTextLink("there"));
        assertThat(linkCallback.getLastCallback(), is(2));
    }
}