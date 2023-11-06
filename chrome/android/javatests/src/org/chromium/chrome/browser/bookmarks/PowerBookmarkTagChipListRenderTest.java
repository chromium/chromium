// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.containsString;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.io.IOException;

/** Tests for the power bookmark experience. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class PowerBookmarkTagChipListRenderTest extends BlankUiTestActivityTestCase {
    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();

    private ViewGroup mContentView;
    private PowerBookmarkTagChipList mTagChipList;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContentView = new LinearLayout(getActivity());

                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT);

                    getActivity().setContentView(mContentView, params);
                    mTagChipList =
                            (PowerBookmarkTagChipList)
                                    getActivity()
                                            .getLayoutInflater()
                                            .inflate(
                                                    R.layout.power_bookmark_tag_chip_list,
                                                    mContentView,
                                                    true)
                                            .findViewById(R.id.power_bookmark_tag_chip_list);
                    mTagChipList.setVisibility(View.VISIBLE);
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "https://crbug.com/1394244")
    public void testTagList() throws IOException {
        PowerBookmarkMeta.Builder meta = PowerBookmarkMeta.newBuilder();
        PowerBookmarkMeta.Tag.Builder tag = PowerBookmarkMeta.Tag.newBuilder();
        tag.setDisplayName("foo");
        meta.addTags(tag);
        tag = PowerBookmarkMeta.Tag.newBuilder();
        tag.setDisplayName("bar");
        meta.addTags(tag);
        tag = PowerBookmarkMeta.Tag.newBuilder();
        tag.setDisplayName("baz");
        meta.addTags(tag);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTagChipList.populateTagMapForPowerBookmarkMeta(meta.build());
                    mTagChipList.populateChipListFromCurrentTagMap();
                });

        onView(withText(allOf(containsString("foo")))).check(matches(isDisplayed()));
        onView(withText(allOf(containsString("bar")))).check(matches(isDisplayed()));
        onView(withText(allOf(containsString("baz")))).check(matches(isDisplayed()));
        mRenderTestRule.render(mContentView, "tag_list");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @DisabledTest(message = "https://crbug.com/1429103")
    public void testTagListLarge() throws IOException {
        PowerBookmarkMeta.Builder meta = PowerBookmarkMeta.newBuilder();
        PowerBookmarkMeta.Tag.Builder tag = PowerBookmarkMeta.Tag.newBuilder();
        tag.setDisplayName("heeeeeeelllllllooooooo");
        meta.addTags(tag);
        tag = PowerBookmarkMeta.Tag.newBuilder();
        tag.setDisplayName("wooooooooooorld");
        meta.addTags(tag);
        tag = PowerBookmarkMeta.Tag.newBuilder();
        tag.setDisplayName("this");
        meta.addTags(tag);
        tag = PowerBookmarkMeta.Tag.newBuilder();
        tag.setDisplayName("test");
        meta.addTags(tag);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTagChipList.populateTagMapForPowerBookmarkMeta(meta.build());
                    mTagChipList.populateChipListFromCurrentTagMap();
                });

        onView(withText(allOf(containsString("heeeeeeelllllllooooooo"))))
                .check(matches(isDisplayed()));
        onView(withText(allOf(containsString("wooooooooooorld")))).check(matches(isDisplayed()));
        mRenderTestRule.render(mContentView, "tag_list_large");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTagListTagSelected() throws IOException {
        PowerBookmarkMeta.Builder meta = PowerBookmarkMeta.newBuilder();
        PowerBookmarkMeta.Tag.Builder tag = PowerBookmarkMeta.Tag.newBuilder();
        tag.setDisplayName("foo");
        meta.addTags(tag);
        tag = PowerBookmarkMeta.Tag.newBuilder();
        tag.setDisplayName("bar");
        meta.addTags(tag);
        tag = PowerBookmarkMeta.Tag.newBuilder();
        tag.setDisplayName("baz");
        meta.addTags(tag);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTagChipList.populateTagMapForPowerBookmarkMeta(meta.build());
                    mTagChipList.populateChipListFromCurrentTagMap();
                });

        onView(withText(allOf(containsString("foo")))).check(matches(isDisplayed()));
        onView(withText(allOf(containsString("foo")))).perform(click());
        onView(withText(allOf(containsString("bar")))).check(matches(isDisplayed()));
        onView(withText(allOf(containsString("baz")))).check(matches(isDisplayed()));
        mRenderTestRule.render(mContentView, "tag_list_tag_selected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTagListDuplicate() throws IOException {
        PowerBookmarkMeta.Builder meta = PowerBookmarkMeta.newBuilder();
        PowerBookmarkMeta.Tag.Builder tag = PowerBookmarkMeta.Tag.newBuilder();
        tag.setDisplayName("foo");
        meta.addTags(tag);
        tag = PowerBookmarkMeta.Tag.newBuilder();
        tag.setDisplayName("bar");
        meta.addTags(tag);
        tag = PowerBookmarkMeta.Tag.newBuilder();
        tag.setDisplayName("foo");
        meta.addTags(tag);
        tag = PowerBookmarkMeta.Tag.newBuilder();
        tag.setDisplayName("baz");
        meta.addTags(tag);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTagChipList.populateTagMapForPowerBookmarkMeta(meta.build());
                    mTagChipList.populateChipListFromCurrentTagMap();
                });

        onView(withText(allOf(containsString("foo")))).check(matches(isDisplayed()));
        onView(withText(allOf(containsString("bar")))).check(matches(isDisplayed()));
        onView(withText(allOf(containsString("baz")))).check(matches(isDisplayed()));
        mRenderTestRule.render(mContentView, "tag_list_duplicate");
    }
}
