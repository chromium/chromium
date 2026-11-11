// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.graphics.Rect;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

/** Unit tests for how {@link IncognitoHistoryDisclaimerCoordinator} picks its anchor. */
@RunWith(BaseRobolectricTestRunner.class)
public class IncognitoHistoryDisclaimerCoordinatorAnchorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;

    private final UnownedUserDataHost mUserDataHost = new UnownedUserDataHost();
    private ActivityController<TestActivity> mActivityController;
    private Activity mActivity;
    private FrameLayout mToolbar;
    private View mFirstAnchor;
    private View mSecondAnchor;
    private int[] mAnchorIds;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(TestActivity.class);
        mActivity = mActivityController.get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mActivityController.setup();
        doReturn(mUserDataHost).when(mWindowAndroid).getUnownedUserDataHost();

        mToolbar = new FrameLayout(mActivity);
        mFirstAnchor = new View(mActivity);
        mFirstAnchor.setId(View.generateViewId());
        mSecondAnchor = new View(mActivity);
        mSecondAnchor.setId(View.generateViewId());
        mToolbar.addView(mFirstAnchor);
        mToolbar.addView(mSecondAnchor);
        FrameLayout content = new FrameLayout(mActivity);
        content.addView(mToolbar);
        mActivity.setContentView(content);
        mAnchorIds = new int[] {mFirstAnchor.getId(), mSecondAnchor.getId()};
    }

    @After
    public void tearDown() {
        mActivityController.destroy();
    }

    private View decorView() {
        return mActivity.getWindow().getDecorView();
    }

    @Test
    public void testAnchorsToFirstShownId() {
        assertEquals(
                mFirstAnchor,
                IncognitoHistoryDisclaimerCoordinator.findAnchorView(decorView(), mAnchorIds));
    }

    @Test
    public void testFallsBackToSecondIdWhenFirstIsGone() {
        mFirstAnchor.setVisibility(View.GONE);

        assertEquals(
                mSecondAnchor,
                IncognitoHistoryDisclaimerCoordinator.findAnchorView(decorView(), mAnchorIds));
    }

    @Test
    public void testSkipsVisibleViewUnderInvisibleParent() {
        // Mirrors Hub: the browsing toolbar is hidden but its Incognito indicator stays VISIBLE.
        FrameLayout hiddenToolbar = new FrameLayout(mActivity);
        View hiddenIndicator = new View(mActivity);
        hiddenIndicator.setId(mFirstAnchor.getId());
        hiddenToolbar.addView(hiddenIndicator);
        hiddenToolbar.setVisibility(View.INVISIBLE);
        ((FrameLayout) mToolbar.getParent()).addView(hiddenToolbar, 0);
        mFirstAnchor.setVisibility(View.GONE);

        assertEquals(
                mSecondAnchor,
                IncognitoHistoryDisclaimerCoordinator.findAnchorView(decorView(), mAnchorIds));
    }

    @Test
    public void testFallsBackToContentTopEndWhenNoAnchorIsShown() {
        mToolbar.setVisibility(View.GONE);
        assertNull(IncognitoHistoryDisclaimerCoordinator.findAnchorView(decorView(), mAnchorIds));

        IncognitoHistoryDisclaimerCoordinator coordinator =
                new IncognitoHistoryDisclaimerCoordinator(mActivity, mWindowAndroid, mAnchorIds);
        RectProvider rectProvider = coordinator.createAnchorRectProvider(decorView());
        coordinator.destroy();

        assertFalse(rectProvider instanceof ViewRectProvider);
        View content = mActivity.findViewById(android.R.id.content);
        int[] location = new int[2];
        content.getLocationOnScreen(location);
        Rect rect = rectProvider.getRect();
        assertEquals(location[0] + content.getWidth(), rect.left);
        assertEquals(location[1], rect.top);
        assertTrue(rect.isEmpty());
    }

    @Test
    public void testUsesViewRectProviderForShownAnchor() {
        IncognitoHistoryDisclaimerCoordinator coordinator =
                new IncognitoHistoryDisclaimerCoordinator(mActivity, mWindowAndroid, mAnchorIds);
        RectProvider rectProvider = coordinator.createAnchorRectProvider(decorView());
        coordinator.destroy();

        assertTrue(rectProvider instanceof ViewRectProvider);
    }
}
