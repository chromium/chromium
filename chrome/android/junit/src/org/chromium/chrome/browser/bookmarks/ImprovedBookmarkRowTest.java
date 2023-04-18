// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link BookmarkToolbarMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImprovedBookmarkRowTest {
    private static final String TITLE = "Test title";
    private static final String DESCRIPTION = "Test description";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock
    Drawable mIcon;
    @Mock
    View mAccessoryView;
    @Mock
    ListMenu mListMenu;
    @Mock
    Runnable mPopupListener;
    @Mock
    Runnable mOpenBookmarkCallback;

    Activity mActivity;
    ImprovedBookmarkRow mImprovedBookmarkRow;
    PropertyModel mModel;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mImprovedBookmarkRow = ImprovedBookmarkRow.buildView(mActivity, /*isVisual=*/true);
        mModel = new PropertyModel.Builder(ImprovedBookmarkRowProperties.ALL_KEYS)
                         .with(ImprovedBookmarkRowProperties.TITLE, TITLE)
                         .with(ImprovedBookmarkRowProperties.DESCRIPTION, DESCRIPTION)
                         .with(ImprovedBookmarkRowProperties.ICON, mIcon)
                         .with(ImprovedBookmarkRowProperties.ACCESSORY_VIEW, mAccessoryView)
                         .with(ImprovedBookmarkRowProperties.LIST_MENU, mListMenu)
                         .with(ImprovedBookmarkRowProperties.POPUP_LISTENER, mPopupListener)
                         .with(ImprovedBookmarkRowProperties.OPEN_BOOKMARK_CALLBACK,
                                 mOpenBookmarkCallback)
                         .with(ImprovedBookmarkRowProperties.EDITABLE, true)
                         .build();

        PropertyModelChangeProcessor.create(
                mModel, mImprovedBookmarkRow, ImprovedBookmarkRowViewBinder::bind);
    }

    @Test
    public void testTitleAndDescription() {
        Assert.assertEquals(
                TITLE, ((TextView) mImprovedBookmarkRow.findViewById(R.id.title)).getText());
        Assert.assertEquals(DESCRIPTION,
                ((TextView) mImprovedBookmarkRow.findViewById(R.id.description)).getText());
    }

    @Test
    public void testNullAccessoryViewClearsExistingViews() {
        TextView tv = new TextView(mActivity);
        mModel.set(ImprovedBookmarkRowProperties.ACCESSORY_VIEW, tv);
        Assert.assertEquals(0,
                ((ViewGroup) mImprovedBookmarkRow.findViewById(R.id.custom_content_container))
                        .indexOfChild(tv));

        mModel.set(ImprovedBookmarkRowProperties.ACCESSORY_VIEW, null);
        Assert.assertEquals(-1,
                ((ViewGroup) mImprovedBookmarkRow.findViewById(R.id.custom_content_container))
                        .indexOfChild(tv));
    }

    @Test
    public void testSelectedShowsCheck() {
        mModel.set(ImprovedBookmarkRowProperties.SELECTED, true);
        Assert.assertEquals(
                View.VISIBLE, mImprovedBookmarkRow.findViewById(R.id.check_image).getVisibility());
        Assert.assertEquals(
                View.GONE, mImprovedBookmarkRow.findViewById(R.id.more).getVisibility());
    }

    @Test
    public void testUnselectedShowsMore() {
        mModel.set(ImprovedBookmarkRowProperties.SELECTED, false);
        Assert.assertEquals(
                View.GONE, mImprovedBookmarkRow.findViewById(R.id.check_image).getVisibility());
        Assert.assertEquals(
                View.VISIBLE, mImprovedBookmarkRow.findViewById(R.id.more).getVisibility());
    }

    @Test
    public void testSelectionActive() {
        mModel.set(ImprovedBookmarkRowProperties.SELECTION_ACTIVE, true);
        Assert.assertFalse(mImprovedBookmarkRow.findViewById(R.id.more).isClickable());
        Assert.assertFalse(mImprovedBookmarkRow.findViewById(R.id.more).isEnabled());
        Assert.assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO,
                mImprovedBookmarkRow.findViewById(R.id.more).getImportantForAccessibility());
    }

    @Test
    public void testSelectionInactive() {
        mModel.set(ImprovedBookmarkRowProperties.SELECTION_ACTIVE, false);
        Assert.assertTrue(mImprovedBookmarkRow.findViewById(R.id.more).isClickable());
        Assert.assertTrue(mImprovedBookmarkRow.findViewById(R.id.more).isEnabled());
        Assert.assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_YES,
                mImprovedBookmarkRow.findViewById(R.id.more).getImportantForAccessibility());
    }
}
