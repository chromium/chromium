// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import static androidx.core.view.WindowInsetsCompat.Type.navigationBars;
import static androidx.core.view.WindowInsetsCompat.Type.statusBars;

import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Rect;
import android.view.Window;
import android.view.WindowManager.LayoutParams;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;

import java.lang.ref.WeakReference;

/** Tests for {@link DisplayCutoutController} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DisplayCutoutControllerTest {
    private static final Insets INITIAL_STATUS_BAR_INSETS = Insets.of(0, 80, 0, 0);
    private static final Insets INITIAL_NAV_BAR_INSETS = Insets.of(0, 0, 0, 24);
    private static final Insets UPDATED_STATUS_BAR_INSETS = Insets.of(0, 64, 0, 0);
    private static final Insets UPDATED_NAV_BAR_INSETS = Insets.of(0, 0, 0, 16);
    private static final Rect INITIAL_EXPECTED_SAFE_AREA = new Rect(0, 80, 0, 24);
    private static final Rect UPDATED_EXPECTED_SAFE_AREA = new Rect(0, 64, 0, 16);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;

    @Mock private MockWebContents mWebContents;

    @Mock private WindowAndroid mWindowAndroid;

    @Mock private Window mWindow;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor private ArgumentCaptor<WebContentsObserver> mWebContentObserverCaptor;

    @Mock private ChromeActivity mChromeActivity;

    @Mock private InsetObserver mInsetObserver;

    @Mock private BaseCustomTabActivity mCustomTabActivity;

    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;

    @Mock private DisplayCutoutController.Delegate mDelegate;

    private DisplayCutoutTabHelper mDisplayCutoutTabHelper;
    private DisplayCutoutController mController;

    private WeakReference<Activity> mActivityRef;

    private final UserDataHost mTabDataHost = new UserDataHost();

    @Before
    public void setUp() {
        mActivityRef = new WeakReference<>(mChromeActivity);

        when(mChromeActivity.getWindow()).thenReturn(mWindow);
        when(mWindow.getAttributes()).thenReturn(new LayoutParams());
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUserDataHost()).thenReturn(mTabDataHost);
        when(mWebContents.isFullscreenForCurrentTab()).thenReturn(true);
        when(mWindowAndroid.getActivity()).thenReturn(mActivityRef);
        when(mWindowAndroid.getInsetObserver()).thenReturn(mInsetObserver);

        // Common defaults for Delegate-based tests. Individual tests still set test-specific
        // values such as getDisplayMode() and override these when needed.
        when(mDelegate.getWebContents()).thenReturn(mWebContents);
        when(mDelegate.isDrawEdgeToEdgeEnabled()).thenReturn(true);
        when(mDelegate.isInteractable()).thenReturn(true);

        ActivityDisplayCutoutModeSupplier.setInstanceForTesting(0);

        mDisplayCutoutTabHelper = spy(new DisplayCutoutTabHelper(mTab));
        mController = spy(mDisplayCutoutTabHelper.mCutoutController);
        mDisplayCutoutTabHelper.mCutoutController = mController;
    }

    @Test
    @SmallTest
    public void testViewportFitUpdate() {
        verify(mController, never()).maybeUpdateLayout();

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER);
        verify(mController).maybeUpdateLayout();
    }

    @Test
    @SmallTest
    public void testViewportFitUpdateOnFullscreen() {
        // Re-adding observers; otherwise, the internal observers are bound to un-mocked
        // mController.
        mController.destroy();
        mController.maybeAddObservers();

        verify(mWebContents, times(2)).addObserver(mWebContentObserverCaptor.capture());
        WebContentsObserver webContentsObserver = mWebContentObserverCaptor.getValue();
        webContentsObserver.didToggleFullscreenModeForTab(true, false);
        verify(mController, description("Should update layout when entering fullscreen"))
                .maybeUpdateLayout();

        webContentsObserver.didToggleFullscreenModeForTab(false, false);
        verify(mController, times(2).description("Should update layout when exiting fullscreen"))
                .maybeUpdateLayout();
    }

    @Test
    @SmallTest
    public void testViewportFitAutoUpdateNotChanged() {
        verify(mController, never()).maybeUpdateLayout();

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.AUTO);
        verify(mController, never()).maybeUpdateLayout();
    }

    @Test
    @SmallTest
    public void testViewportFitCoverUpdateWhenValueNotChanged() {
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER);
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER);

        verify(mController, times(2)).maybeUpdateLayout();
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenAutoAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.AUTO);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenCoverAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenCoverForcedAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER_FORCED_BY_USER_AGENT);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenContainAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.CONTAIN);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenCoverInBrowserFullscreenAndNotWebFullscreen() {
        when(mDelegate.getDisplayMode()).thenReturn(DisplayMode.FULLSCREEN);
        when(mWebContents.isFullscreenForCurrentTab()).thenReturn(false);

        DisplayCutoutController controller = new DisplayCutoutController(mDelegate);
        controller.setViewportFit(ViewportFit.COVER);

        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
                controller.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testCutoutModeWhenCoverInStandaloneAndFeatureDisabled() {
        DisplayCutoutController controller = setUpFeatureDisabledWebApp(DisplayMode.STANDALONE);
        controller.setViewportFit(ViewportFit.COVER);

        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                controller.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testCutoutModeWhenCoverInFullscreenAndFeatureDisabled() {
        DisplayCutoutController controller = setUpFeatureDisabledWebApp(DisplayMode.FULLSCREEN);
        controller.setViewportFit(ViewportFit.COVER);

        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
                controller.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testStandaloneForcedCoverRequestsEdgeToEdge() {
        when(mDelegate.getDisplayMode()).thenReturn(DisplayMode.STANDALONE);
        when(mDelegate.isShortEdgesCutoutModeEnabled()).thenReturn(true);

        DisplayCutoutController controller = new DisplayCutoutController(mDelegate);
        controller.setViewportFit(ViewportFit.COVER_FORCED_BY_USER_AGENT);

        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
                controller.computeDisplayCutoutMode());
        verify(mDelegate).setEdgeToEdgeState(true);
    }

    @Test
    @SmallTest
    public void testBrowserFullscreenExplicitCoverRequestsEdgeToEdge() {
        when(mDelegate.getDisplayMode()).thenReturn(DisplayMode.FULLSCREEN);
        when(mDelegate.isShortEdgesCutoutModeEnabled()).thenReturn(true);
        when(mWebContents.isFullscreenForCurrentTab()).thenReturn(false);

        DisplayCutoutController controller = new DisplayCutoutController(mDelegate);
        controller.setViewportFit(ViewportFit.COVER);

        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
                controller.computeDisplayCutoutMode());
        verify(mDelegate).setEdgeToEdgeState(true);
    }

    @Test
    @SmallTest
    public void testStandaloneCoverMergesSafeAreaWithSystemBars() {
        WindowInsetsCompat initialInsets = mock(WindowInsetsCompat.class);
        WindowInsetsCompat updatedInsets = mock(WindowInsetsCompat.class);
        WindowInsetsCompat zeroInsets = mock(WindowInsetsCompat.class);

        when(initialInsets.getInsetsIgnoringVisibility(statusBars()))
                .thenReturn(INITIAL_STATUS_BAR_INSETS);
        when(initialInsets.getInsetsIgnoringVisibility(navigationBars()))
                .thenReturn(INITIAL_NAV_BAR_INSETS);

        when(updatedInsets.getInsetsIgnoringVisibility(statusBars()))
                .thenReturn(UPDATED_STATUS_BAR_INSETS);
        when(updatedInsets.getInsetsIgnoringVisibility(navigationBars()))
                .thenReturn(UPDATED_NAV_BAR_INSETS);

        when(zeroInsets.getInsetsIgnoringVisibility(statusBars())).thenReturn(Insets.NONE);
        when(zeroInsets.getInsetsIgnoringVisibility(navigationBars())).thenReturn(Insets.NONE);

        when(mDelegate.getAttachedActivity()).thenReturn(mChromeActivity);
        when(mDelegate.getInsetObserver()).thenReturn(mInsetObserver);
        when(mDelegate.getDisplayMode()).thenReturn(DisplayMode.STANDALONE);
        when(mDelegate.isShortEdgesCutoutModeEnabled()).thenReturn(true);
        when(mWebContents.isFullscreenForCurrentTab()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mInsetObserver.getCurrentSafeArea()).thenReturn(new Rect());
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(initialInsets);

        DisplayCutoutController controller =
                new DisplayCutoutController(mDelegate) {
                    @Override
                    protected float getDipScale() {
                        return 1f;
                    }
                };

        clearInvocations(mWebContents);
        controller.setViewportFit(ViewportFit.COVER);

        ArgumentCaptor<Rect> safeAreaCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(mWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        Assert.assertEquals(INITIAL_EXPECTED_SAFE_AREA, safeAreaCaptor.getValue());

        clearInvocations(mWebContents);
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(updatedInsets);
        controller.setViewportFit(ViewportFit.COVER);

        verify(mWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        Assert.assertEquals(UPDATED_EXPECTED_SAFE_AREA, safeAreaCaptor.getValue());

        clearInvocations(mWebContents);
        controller.onInsetChanged();

        verify(mWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        Assert.assertEquals(UPDATED_EXPECTED_SAFE_AREA, safeAreaCaptor.getValue());

        clearInvocations(mWebContents);
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(zeroInsets);
        controller.setViewportFit(ViewportFit.COVER);

        verify(mWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        Assert.assertEquals(UPDATED_EXPECTED_SAFE_AREA, safeAreaCaptor.getValue());
    }

    @Test
    @SmallTest
    public void testBrowserFullscreenCoverAcquiresAndReleasesEdgeToEdge() {
        when(mDelegate.getDisplayMode()).thenReturn(DisplayMode.FULLSCREEN);
        when(mDelegate.isShortEdgesCutoutModeEnabled()).thenReturn(true);

        DisplayCutoutController controller = new DisplayCutoutController(mDelegate);

        controller.setViewportFit(ViewportFit.COVER);
        verify(mDelegate).setEdgeToEdgeState(true);

        clearInvocations(mDelegate);
        controller.setViewportFit(ViewportFit.AUTO);
        verify(mDelegate).setEdgeToEdgeState(false);
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenAutoAndNotInteractable() {
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.AUTO);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenCoverAndNotInteractable() {
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenCoverForcedAndNotInteractable() {
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER_FORCED_BY_USER_AGENT);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenContainAndNotInteractable() {
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.CONTAIN);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testLayoutOnInteractability_True() {
        // In this test we are checking for a side effect of maybeUpdateLayout.
        // This is because the tab observer holds a reference to the original
        // mDisplayCutoutTabHelper and not the spied one.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        reset(mTab);

        mTabObserverCaptor.getValue().onInteractabilityChanged(mTab, true);
        verify(mWindow).getAttributes();
    }

    @Test
    @SmallTest
    public void testLayoutOnInteractability_False() {
        // In this test we are checking for a side effect of maybeUpdateLayout.
        // This is because the tab observer holds a reference to the original
        // mDisplayCutoutTabHelper and not the spied one.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        reset(mTab);

        mTabObserverCaptor.getValue().onInteractabilityChanged(mTab, false);
        verify(mWindow).getAttributes();
    }

    @Test
    @SmallTest
    public void testLayout_NoWindow() {
        // Verify there's no crash when the tab's interactability changes after activity detachment.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        reset(mTab);

        mTabObserverCaptor.getValue().onActivityAttachmentChanged(mTab, null);
        mTabObserverCaptor.getValue().onInteractabilityChanged(mTab, false);
        verify(mWindow, never()).getAttributes();
    }

    @Test
    @SmallTest
    public void testLayoutOnShown() {
        // In this test we are checking for a side effect of maybeUpdateLayout.
        // This is because the tab observer holds a reference to the original
        // mDisplayCutoutTabHelper and not the spied one.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        reset(mTab);

        mTabObserverCaptor.getValue().onShown(mTab, TabSelectionType.FROM_NEW);
        verify(mWindow).getAttributes();
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testGetIsViewportFitCover() {
        // Go through the live creation of DisplayCutoutTabHelper.from(Tab) with our mock Tab.
        UserDataHost tabDataHost = new UserDataHost();
        when(mTab.getUserDataHost()).thenReturn(tabDataHost);
        DisplayCutoutTabHelper tabHelper = DisplayCutoutTabHelper.from(mTab);

        // TODO(crbug.com/40279791) Fix: We cannot access DisplayCutoutController#from(Tab)
        // because it's in a different package from this test. Code copied here.
        UserDataHost host = mTab.getUserDataHost();
        DisplayCutoutController liveController = host.getUserData(DisplayCutoutController.class);

        Assert.assertEquals(
                "Something went wrong with DisplayCutoutController construction or fetching an"
                        + " existing one via from().",
                tabHelper.getDisplayCutoutController(),
                liveController);

        liveController.setViewportFit(ViewportFit.AUTO);
        Assert.assertFalse(
                "SafeAreaInsets should have reported isViewportFitCover() false after the"
                        + " controller's setViewportFit to Auto was called.",
                DisplayCutoutController.getSafeAreaInsetsTracker(mTab).isViewportFitCover());

        liveController.setViewportFit(ViewportFit.COVER);
        Assert.assertTrue(
                "DisplayCutoutController.setViewportFit(cover) did not update the SafeAreaInsets"
                        + " isViewportFitCover to true!",
                DisplayCutoutController.getSafeAreaInsetsTracker(mTab).isViewportFitCover());

        liveController.setViewportFit(ViewportFit.COVER_FORCED_BY_USER_AGENT);
        Assert.assertTrue(
                "DisplayCutoutController.setViewportFit(COVER_FORCED_BY_USER_AGENT) did not update"
                        + " the SafeAreaInsets isViewportFitCover to true!",
                DisplayCutoutController.getSafeAreaInsetsTracker(mTab).isViewportFitCover());

        reset(mTab);
    }

    @Test
    public void testSafeAreaConstraint() {
        mDisplayCutoutTabHelper.setSafeAreaConstraint(true);
        DisplayCutoutController.SafeAreaInsetsTracker tracker =
                DisplayCutoutController.getSafeAreaInsetsTracker(mTab);
        Assert.assertNotNull(tracker);
        Assert.assertTrue(
                "SafeAreaConstrain did not pass through to the safe area insets tracker.",
                tracker.hasSafeAreaConstraint());

        mDisplayCutoutTabHelper.setSafeAreaConstraint(false);
        Assert.assertFalse(
                "SafeAreaConstrain did not pass through to the safe area insets tracker.",
                tracker.hasSafeAreaConstraint());
    }

    @Test
    public void testObserverUpdateOnContentChange() {
        // First, make sure observer is attached at the beginning.
        verify(mWebContents, atLeastOnce()).addObserver(mWebContentObserverCaptor.capture());
        WebContentsObserver observer = mWebContentObserverCaptor.getValue();
        Assert.assertEquals(observer, mController.getWebContentObserverForTesting());

        when(mTab.getWebContents()).thenReturn(null);
        mController.onContentChanged();
        verify(mWebContents).removeObserver(observer);
        Assert.assertNull(mController.getWebContentObserverForTesting());

        clearInvocations(mWebContents);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        mController.onContentChanged();
        verify(mWebContents, atLeastOnce()).addObserver(mWebContentObserverCaptor.capture());
        WebContentsObserver observer2 = mWebContentObserverCaptor.getValue();
        Assert.assertEquals(observer2, mController.getWebContentObserverForTesting());
    }

    @Test
    public void testCreateWithNullWebContent() {
        when(mTab.getWebContents()).thenReturn(null);

        // Reset the controller so we'll need to create a new one.
        mTabDataHost.removeUserData(DisplayCutoutController.class);
        mDisplayCutoutTabHelper = new DisplayCutoutTabHelper(mTab);
        Assert.assertNull(
                mDisplayCutoutTabHelper.mCutoutController.getWebContentObserverForTesting());
    }

    /**
     * Wires up the mTab path through a webapp BaseCustomTabActivity with the given resolved display
     * mode, with the short-edges feature disabled, and returns a fresh controller built via the
     * production ChromeDisplayCutoutDelegate.
     */
    private DisplayCutoutController setUpFeatureDisabledWebApp(@DisplayMode.EnumType int mode) {
        when(mTab.isUserInteractable()).thenReturn(true);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mCustomTabActivity));
        when(mCustomTabActivity.getIntentDataProvider()).thenReturn(mIntentDataProvider);
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.WEBAPP);
        when(mIntentDataProvider.getResolvedDisplayMode()).thenReturn(mode);
        when(mWebContents.isFullscreenForCurrentTab()).thenReturn(false);
        return new DisplayCutoutController(
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab));
    }
}
