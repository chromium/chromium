// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.util.DisplayMetrics;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.overlays.strip.TabLoadTracker.TabLoadTrackerCallback;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripIphController.IphType;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.LocalizationUtils;

/** Unit tests for {@link TabStripIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabStripIphControllerUnitTest {
    private static final float TAB_STRIP_HEIGHT = 40f;
    private static final float TAB_WIDTH = 150f;
    private static final float GROUP_TITLE_WIDTH = 100f;
    private static final int TAB_ID = 2;
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Resources mResources;
    @Mock private Tracker mTracker;
    @Mock private View mContainerView;
    @Mock private StripLayoutView.StripLayoutViewOnClickHandler mClickHandler;
    @Mock private StripLayoutView.StripLayoutViewOnKeyboardFocusHandler mKeyboardFocusHandler;
    @Mock private TabLoadTrackerCallback mLoadTrackerCallback;
    @Mock private LayoutUpdateHost mUpdateHost;

    @Mock
    private StripLayoutGroupTitle.StripLayoutGroupTitleDelegate mStripLayoutGroupTitleDelegate;

    private StripLayoutGroupTitle mGroupTitle;
    private StripLayoutTab mTab;
    private TabStripIphController mController;
    private Context mContext;

    @Before
    public void setUp() {
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.wouldTriggerHelpUi(any())).thenReturn(true);
        TrackerFactory.setTrackerForTests(mTracker);
        mController = new TabStripIphController(mResources, mUserEducationHelper, mTracker);
        DisplayMetrics displayMetrics = new DisplayMetrics();
        displayMetrics.density = 1.f;
        when(mResources.getDisplayMetrics()).thenReturn(displayMetrics);
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mGroupTitle =
                new StripLayoutGroupTitle(
                        mContext,
                        mStripLayoutGroupTitleDelegate,
                        mKeyboardFocusHandler,
                        /* incognito= */ false,
                        TAB_GROUP_ID);
        mTab =
                new StripLayoutTab(
                        mContext,
                        TAB_ID,
                        mClickHandler,
                        mKeyboardFocusHandler,
                        mLoadTrackerCallback,
                        mUpdateHost,
                        /* incognito= */ false,
                        /* isPinned= */ false);
        mGroupTitle.setWidth(GROUP_TITLE_WIDTH);
        mGroupTitle.setHeight(TAB_STRIP_HEIGHT);
        mTab.setWidth(TAB_WIDTH);
    }

    @Test
    public void testIphProperties_TabGroupSync() {
        mController.showIphOnTabStrip(
                mGroupTitle, null, mContainerView, IphType.TAB_GROUP_SYNC, TAB_STRIP_HEIGHT, false);
        var captor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mUserEducationHelper).requestShowIph(captor.capture());
        var cmd = captor.getValue();

        // Assert: feature name and iph string.
        assertEquals(FeatureConstants.TAB_GROUP_SYNC_ON_STRIP_FEATURE, cmd.featureName);
        assertEquals(R.string.newly_synced_tab_group_iph, cmd.stringId);

        // Assert: anchor rect bounds.
        Rect anchorRect = cmd.anchorRect;
        // Group title paddedX (13dp).
        assertEquals("Iph anchor rect left bound is incorrect ", 13, anchorRect.left);
        // Group title width(100f) - title end margin (9dp) = 91dp.
        assertEquals("Iph anchor rect right bound is incorrect ", 91, anchorRect.right);
        // Group title top margin (7dp).
        assertEquals("Iph anchor rect top bound is incorrect ", 7, anchorRect.top);
        // Tab strip height (40dp).
        assertEquals("Iph anchor rect bottom bound is incorrect ", 40, anchorRect.bottom);
    }

    @Test
    public void testIphProperties_GroupTitleBubble() {
        mGroupTitle.setNotificationBubbleShown(true);
        mController.showIphOnTabStrip(
                mGroupTitle,
                null,
                mContainerView,
                IphType.GROUP_TITLE_NOTIFICATION_BUBBLE,
                TAB_STRIP_HEIGHT,
                false);
        var captor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mUserEducationHelper).requestShowIph(captor.capture());
        var cmd = captor.getValue();

        // Assert: feature name and iph string.
        assertEquals(
                FeatureConstants.TAB_GROUP_SHARE_NOTIFICATION_BUBBLE_ON_STRIP_FEATURE,
                cmd.featureName);
        assertEquals(R.string.tab_group_share_notification_bubble_iph, cmd.stringId);

        // Assert: anchor rect bounds.
        Rect anchorRect = cmd.anchorRect;
        // Group title paddedX(13dp) + title width(100dp) - title end padding(8dp) - bubble
        // size(6dp) = 77dp;
        assertEquals("Iph anchor rect left bound is incorrect ", 77, anchorRect.left);
        // Group title width(100f) - title end margin(9dp) - title's end margin(8dp) = 83dp.
        assertEquals("Iph anchor rect right bound is incorrect ", 83, anchorRect.right);
        // Group title top margin(7dp).
        assertEquals("Iph anchor rect top bound is incorrect ", 7, anchorRect.top);
        // Group title height(40dp) - title bottom margin(9dp) = 31dp.
        assertEquals("Iph anchor rect bottom bound is incorrect ", 31, anchorRect.bottom);
    }

    @Test
    public void testIphProperties_GroupTitleBubble_Rtl() {
        LocalizationUtils.setRtlForTesting(true);
        mGroupTitle.setNotificationBubbleShown(true);
        mController.showIphOnTabStrip(
                mGroupTitle,
                null,
                mContainerView,
                IphType.GROUP_TITLE_NOTIFICATION_BUBBLE,
                TAB_STRIP_HEIGHT,
                false);
        var captor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mUserEducationHelper).requestShowIph(captor.capture());
        var cmd = captor.getValue();

        // Assert: feature name and iph string.
        assertEquals(
                FeatureConstants.TAB_GROUP_SHARE_NOTIFICATION_BUBBLE_ON_STRIP_FEATURE,
                cmd.featureName);
        assertEquals(R.string.tab_group_share_notification_bubble_iph, cmd.stringId);

        // Assert: anchor rect bounds.
        Rect anchorRect = cmd.anchorRect;
        // Group title paddedX(9dp) +  title end padding(8dp) = 17dp;
        assertEquals("Iph anchor rect left bound is incorrect.", 17, anchorRect.left);
        // Group title paddedX(9dp) +  title end padding(8dp) + bubble size(6dp) = 23dp.
        assertEquals("Iph anchor rect right bound is incorrect.", 23, anchorRect.right);
        // Group title top margin(7dp).
        assertEquals("Iph anchor rect top bound is incorrect ", 7, anchorRect.top);
        // Group title height(40dp) - title bottom margin(9dp) = 31dp.
        assertEquals("Iph anchor rect bottom bound is incorrect ", 31, anchorRect.bottom);
    }

    @Test
    public void testIphProperties_TabBubble() {
        mGroupTitle.setNotificationBubbleShown(true);
        mController.showIphOnTabStrip(
                mGroupTitle,
                mTab,
                mContainerView,
                IphType.TAB_NOTIFICATION_BUBBLE,
                TAB_STRIP_HEIGHT,
                false);
        var captor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mUserEducationHelper).requestShowIph(captor.capture());
        var cmd = captor.getValue();

        // Assert: feature name and iph string.
        assertEquals(
                FeatureConstants.TAB_GROUP_SHARE_NOTIFICATION_BUBBLE_ON_STRIP_FEATURE,
                cmd.featureName);
        assertEquals(R.string.tab_group_share_notification_bubble_iph, cmd.stringId);

        // Assert: anchor rect bounds.
        Rect anchorRect = cmd.anchorRect;
        // tabX(0dp) + faviconPadding(26) = 26dp;
        assertEquals("Iph anchor rect left bound is incorrect ", 26, anchorRect.left);
        // tabX(0dp) + faviconPadding(26dp) + faviconWidth(16dp) = 42dp.
        assertEquals("Iph anchor rect right bound is incorrect ", 42, anchorRect.right);
        // Group title top margin(7dp).
        assertEquals("Iph anchor rect top bound is incorrect ", 7, anchorRect.top);
        // Group title height(40dp) - title bottom margin(9dp) = 31dp.
        assertEquals("Iph anchor rect bottom bound is incorrect ", 31, anchorRect.bottom);
    }

    @Test
    public void testIphProperties_TabBubble_Rtl() {
        LocalizationUtils.setRtlForTesting(true);
        mGroupTitle.setNotificationBubbleShown(true);
        mController.showIphOnTabStrip(
                mGroupTitle,
                mTab,
                mContainerView,
                IphType.TAB_NOTIFICATION_BUBBLE,
                TAB_STRIP_HEIGHT,
                false);
        var captor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mUserEducationHelper).requestShowIph(captor.capture());
        var cmd = captor.getValue();

        // Assert: feature name and iph string.
        assertEquals(
                FeatureConstants.TAB_GROUP_SHARE_NOTIFICATION_BUBBLE_ON_STRIP_FEATURE,
                cmd.featureName);
        assertEquals(R.string.tab_group_share_notification_bubble_iph, cmd.stringId);

        // Assert: anchor rect bounds.
        Rect anchorRect = cmd.anchorRect;
        // tabX(0dp) + tabWidth(150dp) - faviconPadding(26dp) - faviconSize(16dp) = 108dp;
        assertEquals("Iph anchor rect left bound is incorrect ", 108, anchorRect.left);
        // tabX(0dp) + tabWidth(150dp) - faviconPadding(26dp) = 124dp.
        assertEquals("Iph anchor rect right bound is incorrect ", 124, anchorRect.right);
        // Group title top margin(7dp).
        assertEquals("Iph anchor rect top bound is incorrect ", 7, anchorRect.top);
        // Group title height(40dp) - title bottom margin(9dp) = 31dp.
        assertEquals("Iph anchor rect bottom bound is incorrect ", 31, anchorRect.bottom);
    }

    @Test
    public void testIphProperties_TabTearingXr() {
        mController.showIphOnTabStrip(
                null, mTab, mContainerView, IphType.TAB_TEARING_XR, TAB_STRIP_HEIGHT, true);
        var captor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mUserEducationHelper).requestShowIph(captor.capture());
        var cmd = captor.getValue();

        // Assert: feature name and snooze mode.
        assertEquals(FeatureConstants.IPH_TAB_TEARING_XR, cmd.featureName);
        assertEquals(true, cmd.enableSnoozeMode);
    }
}
