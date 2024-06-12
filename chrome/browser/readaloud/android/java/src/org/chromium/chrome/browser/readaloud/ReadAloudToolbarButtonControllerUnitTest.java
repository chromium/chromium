// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;

/** Unit tests for {@link ReadAloudToolbarButtonController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
public class ReadAloudToolbarButtonControllerUnitTest {

    @Mock private Tab mTab;
    @Mock private Drawable mDrawable;
    @Mock private Tracker mTracker;
    @Mock private ReadAloudController mReadAloudController;
    @Mock private Context mContext;

    private UserActionTester mActionTester;

    private ReadAloudToolbarButtonController mButtonController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActionTester = new UserActionTester();

        when(mContext.getString(anyInt())).thenReturn("String");
        when(mTab.getContext()).thenReturn(mContext);
        when(mReadAloudController.isReadable(mTab)).thenReturn(true);

        mButtonController =
                new ReadAloudToolbarButtonController(
                        mContext,
                        () -> mTab,
                        mDrawable,
                        () -> mReadAloudController,
                        () -> mTracker);
    }

    @After
    public void tearDown() throws Exception {
        mActionTester.tearDown();
    }

    @Test
    public void testButtonData() {
        ButtonData buttonData = mButtonController.get(mTab);

        Assert.assertTrue(buttonData.canShow());
        Assert.assertTrue(buttonData.isEnabled());
        Assert.assertNotNull(buttonData.getButtonSpec());
    }

    @Test
    public void shouldShowButton_noTab() {
        Assert.assertFalse(mButtonController.shouldShowButton(null));
    }

    @Test
    public void shouldShowButton_noReadAloudController() {
        mButtonController =
                new ReadAloudToolbarButtonController(
                        mContext, () -> mTab, mDrawable, () -> null, () -> mTracker);

        Assert.assertFalse(mButtonController.shouldShowButton(mTab));
    }

    @Test
    public void shouldShowButton_tabNotReadable() {
        when(mReadAloudController.isReadable(mTab)).thenReturn(false);

        Assert.assertFalse(mButtonController.shouldShowButton(mTab));
    }

    @Test
    public void shouldShowButton_tabReadable() {
        when(mReadAloudController.isReadable(mTab)).thenReturn(true);

        Assert.assertTrue(mButtonController.shouldShowButton(mTab));
    }

    @Test
    public void onClick_happyPath() {
        mButtonController.onClick(null);

        Assert.assertEquals(1, mActionTester.getActionCount("MobileTopToolbarReadAloudButton"));
        verify(mTracker)
                .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_READ_ALOUD_CLICKED);
    }

    @Test
    public void onClick_readAloudControllerMissing() {
        mButtonController =
                new ReadAloudToolbarButtonController(
                        mContext, () -> mTab, mDrawable, () -> null, () -> mTracker);
        mButtonController.onClick(null);

        Assert.assertEquals(0, mActionTester.getActionCount("MobileTopToolbarReadAloudButton"));
        verify(mTracker, never())
                .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_READ_ALOUD_CLICKED);
    }

    @Test
    public void onClick_trackerMissing() {
        mButtonController =
                new ReadAloudToolbarButtonController(
                        mContext, () -> mTab, mDrawable, () -> mReadAloudController, () -> null);
        mButtonController.onClick(null);

        Assert.assertEquals(1, mActionTester.getActionCount("MobileTopToolbarReadAloudButton"));
        verify(mTracker, never())
                .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_READ_ALOUD_CLICKED);
    }
}
