// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import static androidx.core.view.WindowInsetsCompat.Type.ime;
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
import org.robolectric.Robolectric;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
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
import org.chromium.ui.mojom.VirtualKeyboardMode;

import java.lang.ref.WeakReference;

/** Tests for {@link DisplayCutoutController} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class DisplayCutoutControllerTest {
    // getIntentDataProvider() is otherwise only populated during native initialization.
    private static class TestCustomTabActivity extends CustomTabActivity {
        private BrowserServicesIntentDataProvider mTestIntentDataProvider;

        @Override
        public BrowserServicesIntentDataProvider getIntentDataProvider() {
            return mTestIntentDataProvider;
        }
    }

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
    @Mock private InsetObserver mInsetObserver;
    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock private DisplayCutoutController.Delegate mDelegate;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor private ArgumentCaptor<WebContentsObserver> mWebContentObserverCaptor;

    private final UserDataHost mTabDataHost = new UserDataHost();
    private Activity mActivity;
    private DisplayCutoutTabHelper mDisplayCutoutTabHelper;
    private DisplayCutoutController mController;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();

        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUserDataHost()).thenReturn(mTabDataHost);
        when(mWebContents.isFullscreenForCurrentTab()).thenReturn(true);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
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
    public void testViewportFitUpdate() {
        verify(mController, never()).maybeUpdateLayout();

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER);
        verify(mController).maybeUpdateLayout();
    }

    @Test
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
    public void testViewportFitAutoUpdateNotChanged() {
        verify(mController, never()).maybeUpdateLayout();

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.AUTO);
        verify(mController, never()).maybeUpdateLayout();
    }

    @Test
    public void testViewportFitCoverUpdateWhenValueNotChanged() {
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER);
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER);

        verify(mController, times(2)).maybeUpdateLayout();
    }

    @Test
    public void testCutoutModeWhenAutoAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.AUTO);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                mController.computeDisplayCutoutMode());
    }

    @Test
    public void testCutoutModeWhenCoverAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
                mController.computeDisplayCutoutMode());
    }

    @Test
    public void testCutoutModeWhenCoverForcedAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER_FORCED_BY_USER_AGENT);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
                mController.computeDisplayCutoutMode());
    }

    @Test
    public void testCutoutModeWhenContainAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.CONTAIN);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER,
                mController.computeDisplayCutoutMode());
    }

    @Test
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
    @DisableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testCutoutModeWhenCoverInStandaloneAndFeatureDisabled() {
        DisplayCutoutController controller = setUpFeatureDisabledWebApp(DisplayMode.STANDALONE);
        controller.setViewportFit(ViewportFit.COVER);

        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                controller.computeDisplayCutoutMode());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testCutoutModeWhenCoverInFullscreenAndFeatureDisabled() {
        DisplayCutoutController controller = setUpFeatureDisabledWebApp(DisplayMode.FULLSCREEN);
        controller.setViewportFit(ViewportFit.COVER);

        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
                controller.computeDisplayCutoutMode());
    }

    @Test
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
    public void testStandaloneCoverReleasesEdgeToEdgeWhileNotInteractable() {
        when(mDelegate.getDisplayMode()).thenReturn(DisplayMode.STANDALONE);
        when(mDelegate.isShortEdgesCutoutModeEnabled()).thenReturn(true);

        DisplayCutoutController controller = new DisplayCutoutController(mDelegate);
        controller.setViewportFit(ViewportFit.COVER);
        verify(mDelegate).setEdgeToEdgeState(true);

        // The tab is hidden, e.g. behind a child tab; the edge-to-edge claim is released.
        when(mDelegate.isInteractable()).thenReturn(false);
        controller.maybeUpdateLayout();
        verify(mDelegate).setEdgeToEdgeState(false);

        // The tab becomes interactable again; edge-to-edge is re-applied.
        clearInvocations(mDelegate);
        when(mDelegate.isInteractable()).thenReturn(true);
        controller.maybeUpdateLayout();
        verify(mDelegate).setEdgeToEdgeState(true);
    }

    @Test
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

        // A real soft keyboard is far taller than the navigation bar.
        when(updatedInsets.getInsets(ime())).thenReturn(Insets.of(0, 0, 0, 392));

        when(mDelegate.getAttachedActivity()).thenReturn(mActivity);
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
        when(updatedInsets.isVisible(ime())).thenReturn(true);
        when(mWebContents.getVirtualKeyboardMode()).thenReturn(VirtualKeyboardMode.RESIZES_VISUAL);
        controller.onInsetChanged();

        // A resizes-visual keyboard occludes the navigation bar just like a resizes-content one;
        // fixed bottom UI following the visual viewport must not keep an extra bar-height gap.
        verify(mWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        Assert.assertEquals(
                new Rect(0, UPDATED_STATUS_BAR_INSETS.top, 0, 0), safeAreaCaptor.getValue());

        clearInvocations(mWebContents);
        when(mWebContents.getVirtualKeyboardMode()).thenReturn(VirtualKeyboardMode.RESIZES_CONTENT);
        controller.onInsetChanged();

        verify(mWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        Assert.assertEquals(
                new Rect(0, UPDATED_STATUS_BAR_INSETS.top, 0, 0), safeAreaCaptor.getValue());

        clearInvocations(mWebContents);
        when(mWebContents.getVirtualKeyboardMode())
                .thenReturn(VirtualKeyboardMode.OVERLAYS_CONTENT);
        controller.onInsetChanged();

        verify(mWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        Assert.assertEquals(UPDATED_EXPECTED_SAFE_AREA, safeAreaCaptor.getValue());

        clearInvocations(mWebContents);
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(zeroInsets);
        controller.setViewportFit(ViewportFit.COVER);

        verify(mWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        Assert.assertEquals(UPDATED_EXPECTED_SAFE_AREA, safeAreaCaptor.getValue());
    }

    /**
     * Builds a controller whose window insets report the given navigation bar and IME bottoms with
     * a visible keyboard in the given {@link VirtualKeyboardMode}.
     */
    private DisplayCutoutController createControllerForImeTest(
            int navBarBottom, int imeBottom, @VirtualKeyboardMode.EnumType int keyboardMode) {
        WindowInsetsCompat insets = mock(WindowInsetsCompat.class);
        when(insets.getInsetsIgnoringVisibility(statusBars()))
                .thenReturn(UPDATED_STATUS_BAR_INSETS);
        when(insets.getInsetsIgnoringVisibility(navigationBars()))
                .thenReturn(Insets.of(0, 0, 0, navBarBottom));
        when(insets.getInsets(ime())).thenReturn(Insets.of(0, 0, 0, imeBottom));
        when(insets.isVisible(ime())).thenReturn(true);

        when(mDelegate.getAttachedActivity()).thenReturn(mActivity);
        when(mDelegate.getInsetObserver()).thenReturn(mInsetObserver);
        when(mDelegate.getDisplayMode()).thenReturn(DisplayMode.STANDALONE);
        when(mDelegate.isShortEdgesCutoutModeEnabled()).thenReturn(true);
        when(mWebContents.isFullscreenForCurrentTab()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWebContents.getVirtualKeyboardMode()).thenReturn(keyboardMode);
        when(mInsetObserver.getCurrentSafeArea()).thenReturn(new Rect());
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(insets);

        DisplayCutoutController controller =
                new DisplayCutoutController(mDelegate) {
                    @Override
                    protected float getDipScale() {
                        return 1f;
                    }
                };
        // Browser edge-to-edge, and therefore the browser safe area merge, only applies to a
        // page that asked for viewport-fit=cover.
        controller.setViewportFit(ViewportFit.COVER);
        return controller;
    }

    @Test
    public void testResizesContentImeTallerThanNavBarClearsBottom() {
        // The common case: the keyboard fully covers the navigation bar, so the resized content
        // no longer has anything obstructing its bottom edge.
        DisplayCutoutController controller =
                createControllerForImeTest(
                        /* navBarBottom= */ 16,
                        /* imeBottom= */ 392,
                        VirtualKeyboardMode.RESIZES_CONTENT);

        clearInvocations(mWebContents);
        controller.onInsetChanged();

        ArgumentCaptor<Rect> safeAreaCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(mWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        Assert.assertEquals(
                new Rect(0, UPDATED_STATUS_BAR_INSETS.top, 0, 0), safeAreaCaptor.getValue());
    }

    @Test
    public void testResizesContentShortImeKeepsUncoveredNavigationBar() {
        // Defensive case, e.g. a floating or split keyboard, or a hardware keyboard showing only
        // a suggestion strip: the IME is shorter than the navigation bar, so the part of the bar
        // it does not cover must stay in the safe area.
        DisplayCutoutController controller =
                createControllerForImeTest(
                        /* navBarBottom= */ 16,
                        /* imeBottom= */ 4,
                        VirtualKeyboardMode.RESIZES_CONTENT);

        clearInvocations(mWebContents);
        controller.onInsetChanged();

        ArgumentCaptor<Rect> safeAreaCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(mWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        Assert.assertEquals(
                new Rect(0, UPDATED_STATUS_BAR_INSETS.top, 0, 12), safeAreaCaptor.getValue());
    }

    @Test
    public void testResizesVisualImeTallerThanNavBarClearsBottom() {
        // The default keyboard mode since M108: the visual viewport shrinks above the keyboard
        // while the layout viewport keeps its size. The keyboard still occludes the navigation
        // bar, so env(safe-area-inset-bottom) must not keep the bar inset. crbug.com/407420295.
        DisplayCutoutController controller =
                createControllerForImeTest(
                        /* navBarBottom= */ 16,
                        /* imeBottom= */ 392,
                        VirtualKeyboardMode.RESIZES_VISUAL);

        clearInvocations(mWebContents);
        controller.onInsetChanged();

        ArgumentCaptor<Rect> safeAreaCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(mWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        Assert.assertEquals(
                new Rect(0, UPDATED_STATUS_BAR_INSETS.top, 0, 0), safeAreaCaptor.getValue());
    }

    @Test
    public void testResizesVisualShortImeKeepsUncoveredNavigationBar() {
        DisplayCutoutController controller =
                createControllerForImeTest(
                        /* navBarBottom= */ 16,
                        /* imeBottom= */ 4,
                        VirtualKeyboardMode.RESIZES_VISUAL);

        clearInvocations(mWebContents);
        controller.onInsetChanged();

        ArgumentCaptor<Rect> safeAreaCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(mWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        Assert.assertEquals(
                new Rect(0, UPDATED_STATUS_BAR_INSETS.top, 0, 12), safeAreaCaptor.getValue());
    }

    @Test
    public void testUnsetKeyboardModeImeClearsBottom() {
        // Pages that never declare interactive-widget report UNSET, and the Android platform
        // default for UNSET is resizes-visual, so the keyboard still occludes the bottom edge.
        // This is the configuration real pages hit. crbug.com/407420295.
        DisplayCutoutController controller =
                createControllerForImeTest(
                        /* navBarBottom= */ 48, /* imeBottom= */ 392, VirtualKeyboardMode.UNSET);

        clearInvocations(mWebContents);
        controller.onInsetChanged();

        ArgumentCaptor<Rect> safeAreaCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(mWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        Assert.assertEquals(
                new Rect(0, UPDATED_STATUS_BAR_INSETS.top, 0, 0), safeAreaCaptor.getValue());
    }

    @Test
    public void testOverlaysContentImeKeepsNavigationBarInset() {
        // With the VirtualKeyboard API the author manages keyboard geometry explicitly via
        // env(keyboard-inset-*); the bar inset must stay stable under the keyboard.
        DisplayCutoutController controller =
                createControllerForImeTest(
                        /* navBarBottom= */ 16,
                        /* imeBottom= */ 392,
                        VirtualKeyboardMode.OVERLAYS_CONTENT);

        clearInvocations(mWebContents);
        controller.onInsetChanged();

        ArgumentCaptor<Rect> safeAreaCaptor = ArgumentCaptor.forClass(Rect.class);
        verify(mWebContents).setDisplayCutoutSafeArea(safeAreaCaptor.capture());
        Assert.assertEquals(
                new Rect(0, UPDATED_STATUS_BAR_INSETS.top, 0, 16), safeAreaCaptor.getValue());
    }

    @Test
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
    public void testCutoutModeWhenAutoAndNotInteractable() {
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.AUTO);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                mController.computeDisplayCutoutMode());
    }

    @Test
    public void testCutoutModeWhenCoverAndNotInteractable() {
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                mController.computeDisplayCutoutMode());
    }

    @Test
    public void testCutoutModeWhenCoverForcedAndNotInteractable() {
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER_FORCED_BY_USER_AGENT);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                mController.computeDisplayCutoutMode());
    }

    @Test
    public void testCutoutModeWhenContainAndNotInteractable() {
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.CONTAIN);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                mController.computeDisplayCutoutMode());
    }

    @Test
    public void testCutoutModeChangeRedispatchesInsets() {
        // The cutout mode feeds WindowInsetsUtils#shouldPadDisplayCutout, which decides whether the
        // edge-to-edge layout pads content away from the cutout. Changing the mode has to re-run
        // the inset pass, otherwise the layout keeps the decision made under the previous mode and
        // a page that switched from viewport-fit=cover to a fitted value keeps rendering
        // underneath the cutout.
        when(mDelegate.getDisplayMode()).thenReturn(DisplayMode.STANDALONE);
        when(mDelegate.isShortEdgesCutoutModeEnabled()).thenReturn(true);
        when(mDelegate.getInsetObserver()).thenReturn(mInsetObserver);
        when(mDelegate.getAttachedActivity()).thenReturn(mActivity);

        DisplayCutoutController controller = new DisplayCutoutController(mDelegate);
        controller.onActivityAttachmentChanged(mWindowAndroid);
        LayoutParams attributes = mActivity.getWindow().getAttributes();

        // A cover page draws under the cutout via short edges mode.
        controller.setViewportFit(ViewportFit.COVER);
        Assert.assertEquals(
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
                attributes.layoutInDisplayCutoutMode);
        clearInvocations(mInsetObserver);

        // Switching to a fitted value demotes the window out of short edges mode.
        controller.setViewportFit(ViewportFit.AUTO);

        Assert.assertEquals(
                "Fitted page should leave short edges mode.",
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                attributes.layoutInDisplayCutoutMode);
        verify(mInsetObserver).retriggerOnApplyWindowInsets();
    }

    @Test
    public void testCutoutModeUnchangedDoesNotRedispatchInsets() {
        when(mDelegate.getDisplayMode()).thenReturn(DisplayMode.STANDALONE);
        when(mDelegate.isShortEdgesCutoutModeEnabled()).thenReturn(true);
        when(mDelegate.getInsetObserver()).thenReturn(mInsetObserver);
        when(mDelegate.getAttachedActivity()).thenReturn(mActivity);

        DisplayCutoutController controller = new DisplayCutoutController(mDelegate);
        controller.onActivityAttachmentChanged(mWindowAndroid);
        controller.setViewportFit(ViewportFit.COVER);
        clearInvocations(mInsetObserver);

        // Already in short edges mode for cover, so nothing changes and no work is needed.
        controller.maybeUpdateLayout();

        verify(mInsetObserver, never()).retriggerOnApplyWindowInsets();
    }

    @Test
    public void testLayoutOnInteractability_True() {
        // In this test we are checking for a side effect of maybeUpdateLayout.
        // This is because the tab observer holds a reference to the original
        // mDisplayCutoutTabHelper and not the spied one.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        reset(mTab);

        setWindowCutoutMode(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER);
        mTabObserverCaptor.getValue().onInteractabilityChanged(mTab, true);
        assertWindowCutoutMode(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    @Test
    public void testLayoutOnInteractability_False() {
        // In this test we are checking for a side effect of maybeUpdateLayout.
        // This is because the tab observer holds a reference to the original
        // mDisplayCutoutTabHelper and not the spied one.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        reset(mTab);

        setWindowCutoutMode(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER);
        mTabObserverCaptor.getValue().onInteractabilityChanged(mTab, false);
        assertWindowCutoutMode(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    @Test
    public void testLayout_NoWindow() {
        // Verify there's no crash when the tab's interactability changes after activity detachment.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        reset(mTab);

        mTabObserverCaptor.getValue().onActivityAttachmentChanged(mTab, null);
        setWindowCutoutMode(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER);
        mTabObserverCaptor.getValue().onInteractabilityChanged(mTab, false);
        assertWindowCutoutMode(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER);
    }

    @Test
    public void testLayoutOnShown() {
        // In this test we are checking for a side effect of maybeUpdateLayout.
        // This is because the tab observer holds a reference to the original
        // mDisplayCutoutTabHelper and not the spied one.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        reset(mTab);

        setWindowCutoutMode(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER);
        mTabObserverCaptor.getValue().onShown(mTab, TabSelectionType.FROM_NEW);
        assertWindowCutoutMode(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    @Test
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

    private void setWindowCutoutMode(int mode) {
        Window window = mActivity.getWindow();
        LayoutParams attributes = window.getAttributes();
        attributes.layoutInDisplayCutoutMode = mode;
        window.setAttributes(attributes);
    }

    private void assertWindowCutoutMode(int expectedMode) {
        Assert.assertEquals(
                expectedMode, mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode);
    }

    /**
     * Wires up the mTab path through a webapp BaseCustomTabActivity with the given resolved display
     * mode, with the short-edges feature disabled, and returns a fresh controller built via the
     * production ChromeDisplayCutoutDelegate.
     */
    private DisplayCutoutController setUpFeatureDisabledWebApp(@DisplayMode.EnumType int mode) {
        when(mTab.isUserInteractable()).thenReturn(true);
        TestCustomTabActivity customTabActivity =
                Robolectric.buildActivity(TestCustomTabActivity.class).get();
        customTabActivity.mTestIntentDataProvider = mIntentDataProvider;
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(customTabActivity));
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.WEBAPP);
        when(mIntentDataProvider.getResolvedDisplayMode()).thenReturn(mode);
        when(mWebContents.isFullscreenForCurrentTab()).thenReturn(false);
        return new DisplayCutoutController(
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab));
    }
}
