// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.hasTypefaceSpan;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.openTextLink;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.graphics.Typeface;
import android.support.test.InstrumentationRegistry;
import android.text.SpannedString;
import android.text.style.StyleSpan;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.AssistantTextUtils;

/** Tests for {@code AssistantTextUtils}. */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantTextUtilsTest {
    private LinearLayout mTestLayout;

    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

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

    @Before
    public void setUp() {
        mTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), "about:blank"));
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
            AssistantTextUtils.applyVisualAppearanceTags(
                    view, "regular text, <b>bold text</b>", null);
            return view;
        });
        int boldStart = "regular text, ".length();
        int boldEnd = "regular text, bold text".length() - 1;
        onView(withText("regular text, bold text"))
                .check(matches(hasTypefaceSpan(boldStart, boldEnd, Typeface.BOLD)));
        onView(withText("regular text, bold text"))
                .check(matches(not(hasTypefaceSpan(0, boldStart, Typeface.BOLD))));

        TextView italicTextView = runOnUiThreadBlocking(() -> {
            TextView view = new TextView(mTestRule.getActivity());
            mTestLayout.addView(view);
            AssistantTextUtils.applyVisualAppearanceTags(
                    view, "regular text, <i>italic text</i>", null);
            return view;
        });
        int italicStart = "italic text, ".length();
        int italicEnd = "italic text, bold text".length() - 1;
        onView(withText("regular text, italic text"))
                .check(matches(hasTypefaceSpan(italicStart, italicEnd, Typeface.ITALIC)));
        onView(withText("regular text, italic text"))
                .check(matches(not(hasTypefaceSpan(0, italicStart, Typeface.ITALIC))));
    }

    @Test
    @MediumTest
    public void testMismatchingTextTags() throws Exception {
        TextView textView = runOnUiThreadBlocking(() -> {
            TextView view = new TextView(mTestRule.getActivity());
            mTestLayout.addView(view);
            AssistantTextUtils.applyVisualAppearanceTags(
                    view, "<b>Fat</b>. <b>Not fat</br>. <i>Italic</i>. <i>Not italic</ii>.", null);
            StyleSpan[] spans =
                    ((SpannedString) view.getText()).getSpans(0, view.length(), StyleSpan.class);
            assertThat(spans.length, is(2));
            assertThat(spans[0].getStyle(), is(Typeface.BOLD));
            assertThat(spans[1].getStyle(), is(Typeface.ITALIC));
            return view;
        });
        onView(is(textView))
                .check(matches(withText("Fat. <b>Not fat</br>. Italic. <i>Not italic</ii>.")));
    }

    @Test
    @MediumTest
    public void testTextLinks() throws Exception {
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

    @Test
    @MediumTest
    public void testMismatchingTextLinks() throws Exception {
        LinkCallback linkCallback = new LinkCallback();

        TextView singleLinkView = runOnUiThreadBlocking(() -> {
            TextView view = new TextView(mTestRule.getActivity());
            mTestLayout.addView(view);
            AssistantTextUtils.applyVisualAppearanceTags(view,
                    "Don't click <link0>here</link1> or <link2>this</lin2>, "
                            + "click <link3>me</link3>.",
                    linkCallback);
            return view;
        });
        onView(is(singleLinkView))
                .check(matches(withText(
                        "Don't click <link0>here</link1> or <link2>this</lin2>, click me.")));
        onView(is(singleLinkView)).perform(openTextLink("me"));
        assertThat(linkCallback.getLastCallback(), is(3));
    }
}