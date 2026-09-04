// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.withSettings;

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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.GURL;

/** Unit tests for {@link TabSharingToolbarUiCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSharingToolbarUiCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private Tab mTab;
    @Mock private Profile mWindowProfile;
    @Mock private TabSharingUiBridge mBridge;
    private WebContents mCapturer;
    @Mock private WebContents mCapturee;
    @Mock private Profile mSessionProfile;
    @Mock private UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock private MediaCaptureDevicesDispatcherAndroid.Natives mMediaCaptureJniMock;

    private final ActivityTabProvider mTabProvider = new ActivityTabProvider();
    private FrameLayout mParentView;
    private TabSharingToolbarUiCoordinator mCoordinator;

    @Before
    public void setUp() {
        MediaCaptureDevicesDispatcherAndroidJni.setInstanceForTesting(mMediaCaptureJniMock);
        mCapturer =
                mock(
                        WebContents.class,
                        withSettings().extraInterfaces(WebContentsObserver.Observable.class));
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
        MediaCaptureDevicesDispatcherAndroid.setSourceSwitchingInProgress(mCapturer, false);
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
        TabSharingUiBridge bridge2 = Mockito.mock(TabSharingUiBridge.class);
        when(bridge2.getCapturer()).thenReturn(mCapturer);
        when(bridge2.getCapturee()).thenReturn(mCapturee);
        mCoordinator.onSharingSessionStarted(mBridge);
        mCoordinator.onSharingSessionStarted(bridge2);
        mCoordinator.destroy();
    }

    @Test
    public void testSourceSwitch_RetainsToolbarAndSwapsInPlace() {
        when(mWindowProfile.isOffTheRecord()).thenReturn(false);
        when(mSessionProfile.isOffTheRecord()).thenReturn(false);

        mCoordinator.onSharingSessionStarted(mBridge);
        TabSharingToolbarContainer container =
                (TabSharingToolbarContainer) mParentView.getChildAt(0);
        assertEquals(1, container.getChildCount());
        android.view.View originalView = container.getChildAt(0);

        // Mark a source switch in progress for this capturer.
        MediaCaptureDevicesDispatcherAndroid.setSourceSwitchingInProgress(mCapturer, true);

        // The old session stops as part of the switch: the toolbar must be retained (not removed)
        // so the reported height does not collapse to zero.
        mCoordinator.onSharingSessionStopped(mBridge);
        assertEquals(1, container.getChildCount());
        assertEquals(originalView, container.getChildAt(0));

        // The new session starts: the toolbar is swapped in place, still a single child.
        TabSharingUiBridge newBridge = Mockito.mock(TabSharingUiBridge.class);
        when(newBridge.getCapturer()).thenReturn(mCapturer);
        when(newBridge.getCapturee()).thenReturn(mCapturee);
        mCoordinator.onSharingSessionStarted(newBridge);
        assertEquals(1, container.getChildCount());
        org.junit.Assert.assertNotSame(originalView, container.getChildAt(0));

        // Ending the (non-switching) session now removes the toolbar normally.
        MediaCaptureDevicesDispatcherAndroid.setSourceSwitchingInProgress(mCapturer, false);
        mCoordinator.onSharingSessionStopped(newBridge);
        assertEquals(0, container.getChildCount());
    }

    @Test
    public void testSourceSwitchAborted_ReleasesRetainedToolbarAfterTimeout() {
        when(mWindowProfile.isOffTheRecord()).thenReturn(false);
        when(mSessionProfile.isOffTheRecord()).thenReturn(false);

        mCoordinator.onSharingSessionStarted(mBridge);
        TabSharingToolbarContainer container =
                (TabSharingToolbarContainer) mParentView.getChildAt(0);
        assertEquals(1, container.getChildCount());

        MediaCaptureDevicesDispatcherAndroid.setSourceSwitchingInProgress(mCapturer, true);
        mCoordinator.onSharingSessionStopped(mBridge);
        // Toolbar retained while the (never-arriving) replacement is awaited.
        assertEquals(1, container.getChildCount());

        // The switch is aborted; the safety timeout should tear down the retained toolbar.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(0, container.getChildCount());
    }
}
