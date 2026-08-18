// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Unit tests for {@link TabSharingToolbarUiCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSharingToolbarUiCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private Tab mTab;
    @Mock private Profile mWindowProfile;
    @Mock private TabSharingUIBridge mBridge;
    @Mock private WebContents mCapturer;
    @Mock private WebContents mCapturee;
    @Mock private Profile mSessionProfile;
    @Mock private UrlFormatter.Natives mUrlFormatterJniMock;

    private final ActivityTabProvider mTabProvider = new ActivityTabProvider();
    private FrameLayout mParentView;
    private TabSharingToolbarUiCoordinator mCoordinator;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mParentView = new FrameLayout(activity);
        mTabProvider.setForTesting(mTab);
        when(mTab.getProfile()).thenReturn(mWindowProfile);
        when(mBridge.getCapturer()).thenReturn(mCapturer);
        when(mBridge.getCapturee()).thenReturn(mCapturee);
        when(mCapturer.getLastCommittedUrl()).thenReturn(GURL.emptyGURL());
        when(mCapturee.getLastCommittedUrl()).thenReturn(GURL.emptyGURL());
        Profile.setProfileFromWebContentsForTesting(mSessionProfile);

        UrlFormatterJni.setInstanceForTesting(mUrlFormatterJniMock);
        when(mUrlFormatterJniMock.formatUrlForSecurityDisplay(any(), anyInt()))
                .thenReturn("example.com");

        mCoordinator =
                new TabSharingToolbarUiCoordinator(
                        activity, mParentView, mTopControlsStacker, mTabProvider);
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
        Profile.setProfileFromWebContentsForTesting(null);
        UrlFormatterJni.setInstanceForTesting(null);
    }

    @Test
    public void testSessionStartedAndStopped_NormalProfile() {
        when(mWindowProfile.isOffTheRecord()).thenReturn(false);
        when(mSessionProfile.isOffTheRecord()).thenReturn(false);

        mCoordinator.onSharingSessionStarted(mBridge);
        // Container initially has 0 children; after session starts, it adds a toolbar view.
        TabSharingToolbarContainer container =
                (TabSharingToolbarContainer) mParentView.getChildAt(0);
        assertEquals(1, container.getChildCount());

        mCoordinator.onSharingSessionStopped(mBridge);
        assertEquals(0, container.getChildCount());
    }

    @Test
    public void testSessionStarted_IncognitoWindowIgnored() {
        when(mWindowProfile.isOffTheRecord()).thenReturn(true);
        when(mSessionProfile.isOffTheRecord()).thenReturn(false);

        mCoordinator.onSharingSessionStarted(mBridge);
        TabSharingToolbarContainer container =
                (TabSharingToolbarContainer) mParentView.getChildAt(0);
        // Should ignore and add 0 children in Incognito window for normal profile session.
        assertEquals(0, container.getChildCount());
    }

    @Test
    public void testCoordinatorDirectLifecycle() {
        TabSharingToolbarCoordinator coordinator =
                new TabSharingToolbarCoordinator(mParentView.getContext(), mBridge, mTabProvider);
        org.junit.Assert.assertNotNull(coordinator.getView());
        coordinator.destroy();
    }

    @Test
    public void testContainerMethodsAndVisibility() {
        TabSharingToolbarContainer container =
                (TabSharingToolbarContainer) mParentView.getChildAt(0);
        assertEquals(
                TopControlsStacker.TopControlType.TAB_SHARING_TOOLBAR,
                container.getTopControlType());
        assertEquals(0, container.getTopControlHeight());
        assertEquals(
                TopControlsStacker.TopControlVisibility.HIDDEN,
                container.getTopControlVisibility());

        // Add a GONE child view -> should evaluate to HIDDEN
        android.view.View goneView = new android.view.View(mParentView.getContext());
        goneView.setVisibility(android.view.View.GONE);
        container.addToolbar(goneView);
        assertEquals(
                TopControlsStacker.TopControlVisibility.HIDDEN,
                container.getTopControlVisibility());
        container.removeToolbar(goneView);

        // Add a VISIBLE child view -> evaluates to VISIBLE
        android.view.View visibleView = new android.view.View(mParentView.getContext());
        visibleView.setVisibility(android.view.View.VISIBLE);
        container.addToolbar(visibleView);
        assertEquals(
                TopControlsStacker.TopControlVisibility.VISIBLE,
                container.getTopControlVisibility());

        // Test measurement and margin adjustments
        container.measure(
                android.view.View.MeasureSpec.makeMeasureSpec(
                        1000, android.view.View.MeasureSpec.EXACTLY),
                android.view.View.MeasureSpec.makeMeasureSpec(
                        48, android.view.View.MeasureSpec.EXACTLY));
        assertEquals(48, container.getTopControlHeight());
        container.onTopControlLayerHeightChanged(150, 50);
        ViewGroup.MarginLayoutParams mlp =
                (ViewGroup.MarginLayoutParams) container.getLayoutParams();
        assertEquals(102, mlp.topMargin);

        // Destroy while height != 0
        container.destroy();
        assertEquals(0, container.getTopControlHeight());
    }

    @Test
    public void testUiCoordinatorEdgeCases() {
        // Untracked bridge stop -> safe fallthrough
        mCoordinator.onSharingSessionStopped(mBridge);

        // Start session with null profile in tab provider -> safe fallthrough
        mTabProvider.setForTesting(null);
        mCoordinator.onSharingSessionStarted(mBridge);
        mCoordinator.onSharingSessionStopped(mBridge);

        // Start multiple sessions and destroy coordinator directly
        TabSharingUIBridge bridge2 = Mockito.mock(TabSharingUIBridge.class);
        when(bridge2.getCapturer()).thenReturn(mCapturer);
        when(bridge2.getCapturee()).thenReturn(mCapturee);
        mCoordinator.onSharingSessionStarted(mBridge);
        mCoordinator.onSharingSessionStarted(bridge2);
        mCoordinator.destroy();
    }
}
