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
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.CLUSTER_DATA;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DISPLAY_AS_SHARED;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.LEAVE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.OPEN_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.SHARED_IMAGE_TILES_VIEW;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.TITLE_DATA;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
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

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesView;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster.ClusterData;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.MockitoHelper;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;

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
    @Mock FaviconResolver mFaviconResolver;
    @Mock SharedImageTilesView mSharedImageTilesView;

    private Activity mActivity;
    private TabGroupRowView mTabGroupRowView;
    private ViewGroup mTabGroupFaviconCluster;
    private TextView mTitleTextView;
    private TextView mSubtitleTextView;
    private FrameLayout mImageTilesContainer;
    private ListMenuButton mListMenuButton;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        MockitoHelper.doCallback(1, (Callback<Drawable> callback) -> callback.onResult(mDrawable))
                .when(mFaviconResolver)
                .resolve(any(), any());
    }

    private void remakeWithModel(PropertyModel propertyModel) {
        mPropertyModel = propertyModel;
        LayoutInflater inflater = LayoutInflater.from(mActivity);
        mTabGroupRowView =
                (TabGroupRowView)
                        inflater.inflate(
                                R.layout.tab_group_row,
                                /* root= */ null,
                                /* attachToRoot= */ false);
        mTabGroupFaviconCluster = mTabGroupRowView.findViewById(R.id.tab_group_favicon_cluster);
        mTitleTextView = mTabGroupRowView.findViewById(R.id.tab_group_title);
        mSubtitleTextView = mTabGroupRowView.findViewById(R.id.tab_group_subtitle);
        mImageTilesContainer = mTabGroupRowView.findViewById(R.id.image_tiles_container);
        mListMenuButton = mTabGroupRowView.findViewById(R.id.more);
        mTabGroupRowView.setTimeAgoResolverForTesting(mTimeAgoResolver);

        PropertyModelChangeProcessor.create(
                mPropertyModel, mTabGroupRowView, TabGroupRowViewBinder::bind);

        mActivity.setContentView(mTabGroupRowView);
    }

    private <T> void remakeWithProperty(ReadableObjectPropertyKey<T> key, T value) {
        remakeWithModel(new PropertyModel.Builder(ALL_KEYS).with(key, value).build());
    }

    private ClusterData makeCornerData(GURL... urls) {
        List<GURL> firstUrls =
                Arrays.stream(urls)
                        .limit(TabGroupFaviconCluster.CORNER_COUNT)
                        .collect(Collectors.toList());
        return new ClusterData(mFaviconResolver, urls.length, firstUrls);
    }

    private Drawable getImageForCorner(@Corner int corner) {
        View quarter = mTabGroupFaviconCluster.getChildAt(corner);
        ImageView imageView = quarter.findViewById(R.id.favicon_image);
        return imageView.getDrawable();
    }

    private CharSequence getTextForCorner(@Corner int corner) {
        View quarter = mTabGroupFaviconCluster.getChildAt(corner);
        TextView textView = quarter.findViewById(R.id.hidden_tab_count);
        return textView.getText();
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
    public void setClusterData_one() {
        remakeWithProperty(CLUSTER_DATA, makeCornerData(JUnitTestGURLs.URL_1));
        assertEquals(mDrawable, getImageForCorner(Corner.TOP_LEFT));

        remakeWithProperty(CLUSTER_DATA, makeCornerData());
        assertNull(getImageForCorner(Corner.TOP_LEFT));

        remakeWithProperty(CLUSTER_DATA, makeCornerData(JUnitTestGURLs.URL_1));
        assertEquals(mDrawable, getImageForCorner(Corner.TOP_LEFT));
    }

    @Test
    public void setClusterData_two() {
        remakeWithProperty(
                CLUSTER_DATA, makeCornerData(JUnitTestGURLs.URL_1, JUnitTestGURLs.URL_2));
        assertEquals(mDrawable, getImageForCorner(Corner.TOP_RIGHT));
    }

    @Test
    public void setClusterData_three() {
        remakeWithProperty(
                CLUSTER_DATA,
                makeCornerData(JUnitTestGURLs.URL_1, JUnitTestGURLs.URL_2, JUnitTestGURLs.URL_3));
        assertEquals(mDrawable, getImageForCorner(Corner.BOTTOM_LEFT));
    }

    @Test
    public void setClusterData_four() {
        remakeWithProperty(
                CLUSTER_DATA,
                makeCornerData(
                        JUnitTestGURLs.URL_1,
                        JUnitTestGURLs.URL_2,
                        JUnitTestGURLs.URL_3,
                        JUnitTestGURLs.BLUE_1));
        assertEquals(mDrawable, getImageForCorner(Corner.BOTTOM_RIGHT));
        assertEquals("", getTextForCorner(Corner.BOTTOM_RIGHT));
    }

    @Test
    public void setClusterData_five() {
        remakeWithProperty(
                CLUSTER_DATA,
                makeCornerData(
                        JUnitTestGURLs.URL_1,
                        JUnitTestGURLs.URL_2,
                        JUnitTestGURLs.URL_3,
                        JUnitTestGURLs.BLUE_1,
                        JUnitTestGURLs.BLUE_2));
        assertNull(getImageForCorner(Corner.BOTTOM_RIGHT));
        assertTrue(getTextForCorner(Corner.BOTTOM_RIGHT).toString().contains("2"));
    }

    @Test
    public void testContentDescriptions() {
        remakeWithProperty(TITLE_DATA, new Pair<>("Title", 3));
        assertEquals("Open Title", mTitleTextView.getContentDescription());
        assertEquals("Title tab group options", mListMenuButton.getContentDescription());
    }

    @Test
    public void testDisplayAsShared() {
        remakeWithModel(new PropertyModel.Builder(ALL_KEYS).with(DISPLAY_AS_SHARED, true).build());
        assertEquals(View.VISIBLE, mImageTilesContainer.getVisibility());
        remakeWithModel(new PropertyModel.Builder(ALL_KEYS).with(DISPLAY_AS_SHARED, false).build());
        assertEquals(View.GONE, mImageTilesContainer.getVisibility());
    }

    @Test
    public void testImageTileContainerCallback() {
        remakeWithProperty(SHARED_IMAGE_TILES_VIEW, mSharedImageTilesView);
        assertEquals(1, mImageTilesContainer.getChildCount());
        remakeWithProperty(SHARED_IMAGE_TILES_VIEW, null);
        assertEquals(0, mImageTilesContainer.getChildCount());
    }
}
