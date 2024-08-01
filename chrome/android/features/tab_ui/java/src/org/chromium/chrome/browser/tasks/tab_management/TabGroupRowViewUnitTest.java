// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_LEFT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_RIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_TOP_LEFT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_TOP_RIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.LEAVE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.OPEN_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.PLUS_COUNT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.TITLE_DATA;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.util.Pair;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link TabGroupRowView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupRowViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock TabGroupTimeAgoResolver mTimeAgoResolver;
    @Mock Runnable mRunnable;
    @Mock Drawable mDrawable;

    private Activity mActivity;
    private TabGroupRowView mTabGroupRowView;
    private ViewGroup mTabGroupStartIconParent;
    private TextView mTitleTextView;
    private TextView mSubtitleTextView;
    private ListMenuButton mListMenuButton;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity -> mActivity = activity));
    }

    private void remakeWithModel(PropertyModel propertyModel) {
        mPropertyModel = propertyModel;
        LayoutInflater inflater = LayoutInflater.from(mActivity);
        mTabGroupRowView = (TabGroupRowView) inflater.inflate(R.layout.tab_group_row, null, false);
        mTabGroupStartIconParent = mTabGroupRowView.findViewById(R.id.tab_group_start_icon);
        mTitleTextView = mTabGroupRowView.findViewById(R.id.tab_group_title);
        mSubtitleTextView = mTabGroupRowView.findViewById(R.id.tab_group_subtitle);
        mListMenuButton = mTabGroupRowView.findViewById(R.id.more);
        mTabGroupRowView.setTimeAgoResolverForTesting(mTimeAgoResolver);

        PropertyModelChangeProcessor.create(
                mPropertyModel, mTabGroupRowView, new TabGroupRowViewBinder());

        mActivity.setContentView(mTabGroupRowView);
    }

    private <T> void remakeWithProperty(ReadableObjectPropertyKey<T> key, T value) {
        remakeWithModel(new PropertyModel.Builder(ALL_KEYS).with(key, value).build());
    }

    @Test
    public void testSetTitleData() {
        remakeWithProperty(TITLE_DATA, new Pair<>("Title", 3));
        assertEquals("Title", mTitleTextView.getText());

        remakeWithProperty(TITLE_DATA, new Pair<>(" ", 3));
        assertEquals(" ", mTitleTextView.getText());

        remakeWithProperty(TITLE_DATA, new Pair<>("", 3));
        assertEquals("3 tabs", mTitleTextView.getText());

        remakeWithProperty(TITLE_DATA, new Pair<>(null, 3));
        assertEquals("3 tabs", mTitleTextView.getText());

        remakeWithProperty(TITLE_DATA, new Pair<>("", 1));
        assertEquals("1 tab", mTitleTextView.getText());
    }

    @Test
    public void testSetCreationMillis() {
        long creationMillis = 123L;
        String timeAgo = "Created just now";
        when(mTimeAgoResolver.resolveTimeAgoText(creationMillis)).thenReturn(timeAgo);

        remakeWithProperty(TabGroupRowProperties.CREATION_MILLIS, creationMillis);

        verify(mTimeAgoResolver).resolveTimeAgoText(creationMillis);
        assertEquals(timeAgo, mSubtitleTextView.getText());
    }

    @Test
    public void testSetOpenRunnable() {
        remakeWithProperty(OPEN_RUNNABLE, mRunnable);
        mTabGroupRowView.performClick();
        verify(mRunnable).run();

        reset(mRunnable);
        remakeWithProperty(OPEN_RUNNABLE, null);
        mTabGroupRowView.performClick();
        verifyNoInteractions(mRunnable);
    }

    @Test
    public void testOpenRunnableFromMenu() {
        remakeWithProperty(OPEN_RUNNABLE, mRunnable);
        mListMenuButton.showMenu();
        onView(withText("Open")).perform(click());
        verify(mRunnable).run();
    }

    @Test
    public void testDeleteRunnableFromMenu() {
        remakeWithProperty(DELETE_RUNNABLE, mRunnable);
        mListMenuButton.showMenu();
        onView(withText("Delete")).perform(click());
        verify(mRunnable).run();
    }

    @Test
    public void testLeaveRunnableFromMenu() {
        remakeWithProperty(LEAVE_RUNNABLE, mRunnable);
        mListMenuButton.showMenu();
        onView(withText("Leave")).perform(click());
        verify(mRunnable).run();
    }

    @Test
    public void testSetFavicon_topLeft() {
        remakeWithProperty(ASYNC_FAVICON_TOP_LEFT, (callback) -> callback.onResult(mDrawable));
        ImageView imageView =
                mTabGroupStartIconParent.getChildAt(0).findViewById(R.id.favicon_image);
        assertEquals(mDrawable, imageView.getDrawable());

        mPropertyModel.set(ASYNC_FAVICON_TOP_LEFT, null);
        assertNull(imageView.getDrawable());

        mPropertyModel.set(ASYNC_FAVICON_TOP_LEFT, (callback) -> callback.onResult(mDrawable));
        assertEquals(mDrawable, imageView.getDrawable());
    }

    @Test
    public void testSetFavicon_topRight() {
        remakeWithProperty(ASYNC_FAVICON_TOP_RIGHT, (callback) -> callback.onResult(mDrawable));
        ImageView imageView =
                mTabGroupStartIconParent.getChildAt(1).findViewById(R.id.favicon_image);
        assertEquals(mDrawable, imageView.getDrawable());
    }

    @Test
    public void testSetFavicon_bottomLeft() {
        remakeWithProperty(ASYNC_FAVICON_BOTTOM_LEFT, (callback) -> callback.onResult(mDrawable));
        ImageView imageView =
                mTabGroupStartIconParent.getChildAt(3).findViewById(R.id.favicon_image);
        assertEquals(mDrawable, imageView.getDrawable());
    }

    @Test
    public void testSetFavicon_bottomRightAndPlusCount() {
        remakeWithProperty(ASYNC_FAVICON_BOTTOM_RIGHT, (callback) -> callback.onResult(mDrawable));
        ImageView imageView =
                mTabGroupStartIconParent.getChildAt(2).findViewById(R.id.favicon_image);
        TextView textView =
                mTabGroupStartIconParent.getChildAt(2).findViewById(R.id.hidden_tab_count);
        assertEquals(mDrawable, imageView.getDrawable());
        assertEquals("", textView.getText());

        mPropertyModel.set(PLUS_COUNT, 321);
        assertEquals(mDrawable, imageView.getDrawable());
        assertEquals("", textView.getText());

        mPropertyModel.set(ASYNC_FAVICON_BOTTOM_RIGHT, null);
        assertNull(imageView.getDrawable());
        assertTrue(textView.getText().toString().contains("321"));

        mPropertyModel.set(PLUS_COUNT, 0);
        assertNull(imageView.getDrawable());
        assertEquals("", textView.getText());
    }

    @Test
    public void testResetOnBind() {
        remakeWithProperty(ASYNC_FAVICON_TOP_LEFT, (callback) -> callback.onResult(mDrawable));
        ImageView imageView =
                mTabGroupStartIconParent.getChildAt(0).findViewById(R.id.favicon_image);
        assertEquals(mDrawable, imageView.getDrawable());

        mTabGroupRowView.resetOnBind();
        assertNull(imageView.getDrawable());
    }

    @Test
    public void testContentDescriptions() {
        remakeWithProperty(TITLE_DATA, new Pair<>("Title", 3));
        assertEquals("Open Title", mTitleTextView.getContentDescription());
        assertEquals("Title tab group options", mListMenuButton.getContentDescription());
    }
}
