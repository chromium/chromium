// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link AddToBookmarksToolbarButtonController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2})
public class AddToBookmarksToolbarButtonControllerUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private WebContents mWebContents;
    @Mock
    private Tab mTab;
    @Mock
    private Drawable mDrawable;
    @Mock
    private Tracker mTracker;
    @Mock
    private TabBookmarker mTabBookmarker;
    @Mock
    private Resources mResources;
    @Mock
    private Context mContext;

    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActionTester = new UserActionTester();

        when(mContext.getResources()).thenReturn(mResources);
        when(mTab.getContext()).thenReturn(mContext);
        when(mTab.getWebContents()).thenReturn(mWebContents);
    }

    @After
    public void tearDown() throws Exception {
        mActionTester.tearDown();
    }

    @Test
    public void testButtonData() {
        AddToBookmarksToolbarButtonController addToBookmarksToolbarButtonController =
                new AddToBookmarksToolbarButtonController(()
                                                                  -> mTab,
                        mDrawable, "Translate button description",
                        () -> mTabBookmarker, () -> mTracker);
        ButtonData buttonData = addToBookmarksToolbarButtonController.get(mTab);

        Assert.assertTrue(buttonData.canShow());
        Assert.assertTrue(buttonData.isEnabled());
        Assert.assertNotNull(buttonData.getButtonSpec());
    }

    @Test
    public void testOnClick() {
        AddToBookmarksToolbarButtonController addToBookmarksToolbarButtonController =
                new AddToBookmarksToolbarButtonController(()
                                                                  -> mTab,
                        mDrawable, "Translate button description",
                        () -> mTabBookmarker, () -> mTracker);
        addToBookmarksToolbarButtonController.onClick(null);

        Assert.assertEquals(
                1, mActionTester.getActionCount("MobileTopToolbarAddToBookmarksButton"));
        verify(mTracker).notifyEvent(
                EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_ADD_TO_BOOKMARKS_OPENED);
        verify(mTabBookmarker).addOrEditBookmark(mTab);
    }
}
