// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.os.Handler;
import android.os.Looper;
import android.view.ActionMode;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.browser.customtabs.CustomTabsIntent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.UserDataHost;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.SimpleHandleStrategy;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar.CustomTabLocationBar;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.status.PageInfoIPHController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup.HistoryDelegate;
import org.chromium.chrome.browser.toolbar.top.ToolbarSnapshotDifference;
import org.chromium.chrome.browser.toolbar.top.ToolbarTablet.OfflineDownloader;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.content_settings.CookieBlocking3pcdStatus;
import org.chromium.components.content_settings.CookieControlsBreakageConfidenceLevel;
import org.chromium.components.content_settings.CookieControlsStatus;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.concurrent.TimeUnit;
import java.util.function.BooleanSupplier;

/** Tests AMP url handling in the CustomTab Toolbar. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowLooper.class, ShadowPostTask.class})
@LooperMode(Mode.PAUSED)
@DisableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
public class CustomTabToolbarUnitTest {
    private static final GURL TEST_URL = JUnitTestGURLs.INITIAL_URL;
    private static final GURL AMP_URL =
            new GURL("https://www.google.com/amp/www.nyt.com/ampthml/blogs.html");
    private static final GURL AMP_CACHE_URL =
            new GURL("https://www.google.com/amp/s/www.nyt.com/ampthml/blogs.html");

    @Rule public MockitoRule mRule = MockitoJUnit.rule();
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock LocationBarModel mLocationBarModel;
    @Mock ActionMode.Callback mActionModeCallback;
    @Mock CustomTabToolbarAnimationDelegate mAnimationDelegate;
    @Mock BrowserStateBrowserControlsVisibilityDelegate mControlsVisibleDelegate;
    @Mock ToolbarDataProvider mToolbarDataProvider;
    @Mock ToolbarTabController mTabController;
    @Mock MenuButtonCoordinator mMenuButtonCoordinator;
    @Mock HistoryDelegate mHistoryDelegate;
    @Mock BooleanSupplier mPartnerHomepageEnabledSupplier;
    @Mock OfflineDownloader mOfflineDownloader;
    @Mock Tab mTab;
    @Mock Callback<Integer> mContainerVisibilityChangeObserver;
    @Mock View mParentView;
    @Mock WindowAndroid mWindowAndroid;
    private @Mock PageInfoIPHController mPageInfoIPHController;

    private Activity mActivity;
    private CustomTabToolbar mToolbar;
    private CustomTabLocationBar mLocationBar;
    private TextView mTitleBar;
    private TextView mUrlBar;

    @Before
    public void setup() {
        ShadowPostTask.setTestImpl(
                (@TaskTraits int taskTraits, Runnable task, long delay) -> {
                    new Handler(Looper.getMainLooper()).postDelayed(task, delay);
                });
        Mockito.doReturn(R.string.accessibility_security_btn_secure)
                .when(mLocationBarModel)
                .getSecurityIconContentDescriptionResourceId();
        Mockito.doReturn(R.color.default_icon_color_tint_list)
                .when(mLocationBarModel)
                .getSecurityIconColorStateList();
        when(mToolbarDataProvider.getTab()).thenReturn(mTab);
        when(mTab.getUserDataHost()).thenReturn(new UserDataHost());
        setUpForUrl(TEST_URL);

        mActivity = Robolectric.buildActivity(TestActivity.class).get();
        mToolbar =
                (CustomTabToolbar)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.custom_tabs_toolbar, null, false);
        mToolbar.initialize(
                mToolbarDataProvider,
                mTabController,
                mMenuButtonCoordinator,
                mHistoryDelegate,
                mPartnerHomepageEnabledSupplier,
                mOfflineDownloader);
        mLocationBar =
                (CustomTabLocationBar)
                        mToolbar.createLocationBar(
                                mLocationBarModel,
                                mActionModeCallback,
                                () -> null,
                                () -> null,
                                mControlsVisibleDelegate,
                                null);
        mUrlBar = mToolbar.findViewById(R.id.url_bar);
        mTitleBar = mToolbar.findViewById(R.id.title_bar);
        mLocationBar.setAnimDelegateForTesting(mAnimationDelegate);
        mLocationBar.setIPHControllerForTesting(mPageInfoIPHController);
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    @Test
    public void testParsesPublisherFromAmp() {
        assertEquals("www.nyt.com", CustomTabToolbar.parsePublisherNameFromUrl(AMP_URL));
        assertEquals("www.nyt.com", CustomTabToolbar.parsePublisherNameFromUrl(AMP_CACHE_URL));
        assertEquals(
                JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                CustomTabToolbar.parsePublisherNameFromUrl(JUnitTestGURLs.EXAMPLE_URL));
    }

    @Test
    public void testToolbarBrandingDelegateImpl_EmptyToRegular() {
        assertUrlAndTitleVisible(/* titleVisible= */ false, /* urlVisible= */ true);
        mLocationBar.showEmptyLocationBar();
        assertUrlAndTitleVisible(/* titleVisible= */ false, /* urlVisible= */ false);

        // Attempt to update title and URL, should noop since location bar is still in empty state.
        mLocationBar.onTitleChanged();
        mLocationBar.onUrlChanged();
        verify(mLocationBarModel, never()).notifySecurityStateChanged();

        mLocationBar.showRegularToolbar();
        assertUrlAndTitleVisible(/* titleVisible= */ false, /* urlVisible= */ true);
        verify(mLocationBarModel).notifyTitleChanged();
        verify(mLocationBarModel).notifySecurityStateChanged();
        verifyBrowserControlVisibleForRequiredDuration();
        // URL bar truncates trailing /.
        assertUrlBarShowingText(TEST_URL.getSpec().replaceAll("/$", ""));
    }

    @Test
    public void testToolbarBrandingDelegateImpl_EmptyToBranding() {
        mLocationBar.setIconTransitionEnabled(true);
        doTestToolbarBrandingDelegateImpl_EmptyToBranding(true);
    }

    @Test
    public void testToolbarBrandingDelegateImpl_EmptyToBranding_DisableTransition() {
        mLocationBar.setIconTransitionEnabled(false);
        doTestToolbarBrandingDelegateImpl_EmptyToBranding(false);
    }

    private void doTestToolbarBrandingDelegateImpl_EmptyToBranding(boolean animateIconTransition) {
        assertUrlAndTitleVisible(/* titleVisible= */ false, /* urlVisible= */ true);
        mLocationBar.showEmptyLocationBar();
        assertUrlAndTitleVisible(/* titleVisible= */ false, /* urlVisible= */ false);

        // Attempt to update title and URL, should noop since location bar is still in empty state.
        mLocationBar.setShowTitle(true);
        mLocationBar.setUrlBarHidden(false);
        verify(mLocationBarModel, never()).notifySecurityStateChanged();

        mLocationBar.showBrandingLocationBar();
        assertUrlAndTitleVisible(/* titleVisible= */ false, /* urlVisible= */ true);
        verify(mAnimationDelegate).updateSecurityButton(anyInt(), eq(animateIconTransition));
        assertBrandingTextShowingOnUrlBar();

        // Attempt to update title and URL to show Title only - should be ignored during branding.
        reset(mLocationBarModel);
        setUpForUrl(TEST_URL);
        mLocationBar.setShowTitle(true);
        mLocationBar.setUrlBarHidden(true);
        verifyNoMoreInteractions(mLocationBarModel);

        // After getting back to regular toolbar, title should become visible now.
        mLocationBar.showRegularToolbar();
        assertUrlAndTitleVisible(/* titleVisible= */ true, /* urlVisible= */ false);
        verify(mLocationBarModel, atLeastOnce()).notifyTitleChanged();
        verify(mLocationBarModel, atLeastOnce()).notifySecurityStateChanged();
        verifyBrowserControlVisibleForRequiredDuration();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
    public void testIsReadyForTextureCapture() {
        CaptureReadinessResult result = mToolbar.isReadyForTextureCapture();
        assertTrue(result.isReady);
        assertEquals(result.snapshotDifference, ToolbarSnapshotDifference.NULL);

        fakeTextureCapture();
        result = mToolbar.isReadyForTextureCapture();
        assertEquals(result.snapshotDifference, ToolbarSnapshotDifference.NONE);
        assertFalse(result.isReady);

        when(mToolbarDataProvider.getPrimaryColor()).thenReturn(Color.RED);
        mToolbar.onPrimaryColorChanged(false);
        result = mToolbar.isReadyForTextureCapture();
        assertTrue(result.isReady);
        assertEquals(result.snapshotDifference, ToolbarSnapshotDifference.TINT);

        fakeTextureCapture();
        when(mToolbarDataProvider.getTab()).thenReturn(mTab);
        when(mTab.getUserDataHost()).thenReturn(new UserDataHost());
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.RED_1);
        UrlBarData urlBarData = UrlBarData.forUrl(JUnitTestGURLs.RED_1);
        when(mLocationBarModel.getUrlBarData()).thenReturn(urlBarData);
        mLocationBar.onUrlChanged();
        result = mToolbar.isReadyForTextureCapture();
        assertTrue(result.isReady);
        assertEquals(result.snapshotDifference, ToolbarSnapshotDifference.URL_TEXT);

        fakeTextureCapture();
        when(mLocationBarModel.hasTab()).thenReturn(true);
        when(mLocationBarModel.getTitle()).thenReturn("Red 1");
        mLocationBar.onTitleChanged();
        result = mToolbar.isReadyForTextureCapture();
        assertTrue(result.isReady);
        assertEquals(result.snapshotDifference, ToolbarSnapshotDifference.TITLE_TEXT);

        fakeTextureCapture();
        when(mLocationBarModel.getSecurityIconResource(anyBoolean()))
                .thenReturn(R.drawable.ic_globe_24dp);
        when(mAnimationDelegate.getSecurityIconRes()).thenReturn(R.drawable.ic_globe_24dp);
        mLocationBar.onSecurityStateChanged();
        result = mToolbar.isReadyForTextureCapture();
        assertTrue(result.isReady);
        assertEquals(result.snapshotDifference, ToolbarSnapshotDifference.SECURITY_ICON);

        fakeTextureCapture();
        when(mAnimationDelegate.isInAnimation()).thenReturn(true);
        result = mToolbar.isReadyForTextureCapture();
        assertTrue(result.isReady);
        assertEquals(result.snapshotDifference, ToolbarSnapshotDifference.CCT_ANIMATION);

        when(mAnimationDelegate.isInAnimation()).thenReturn(false);
        fakeTextureCapture();
        mToolbar.layout(0, 0, 100, 100);
        result = mToolbar.isReadyForTextureCapture();
        assertTrue(result.isReady);
        assertEquals(result.snapshotDifference, ToolbarSnapshotDifference.LOCATION_BAR_WIDTH);
    }

    @Test
    public void testAboutBlankUrlIsShown() {
        setUpForAboutBlank();
        ShadowLooper.idleMainLooper();
        mLocationBar.onUrlChanged();
        assertEquals("The url bar should be visible.", View.VISIBLE, mUrlBar.getVisibility());
        assertEquals(
                "The url bar should show about:blank",
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL,
                mUrlBar.getText().toString());
    }

    @Test
    public void testTitleIsHiddenForAboutBlank() {
        setUpForAboutBlank();
        mLocationBar.setShowTitle(true);
        ShadowLooper.idleMainLooper();
        mLocationBar.onUrlChanged();
        assertEquals("The title should be gone.", View.GONE, mTitleBar.getVisibility());
    }

    @Test
    public void testCannotHideUrlForAboutBlank() {
        setUpForAboutBlank();
        mLocationBar.setUrlBarHidden(true);
        ShadowLooper.idleMainLooper();
        mLocationBar.onUrlChanged();
        assertEquals("The url bar should be visible.", View.VISIBLE, mUrlBar.getVisibility());
        assertEquals(
                "The url bar should show about:blank",
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL,
                mUrlBar.getText().toString());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_SIDE_SHEET})
    public void testMaximizeButton() {
        assertFalse(mToolbar.isMaximizeButtonEnabledForTesting());
        mToolbar.initSideSheetMaximizeButton(/* maximizedOnInit= */ false, () -> true);
        assertTrue(mToolbar.isMaximizeButtonEnabledForTesting());
        var maximizeButton =
                (ImageButton) mToolbar.findViewById(R.id.custom_tabs_sidepanel_maximize);

        mToolbar.onFinishInflate();
        View titleUrlContainer = Mockito.mock(View.class);
        mLocationBar.setTitleUrlContainerForTesting(titleUrlContainer);
        int maximizeButtonWidth =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.location_bar_action_icon_width);
        int titleUrlPaddingEnd =
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_edge_padding);
        int threshold = maximizeButtonWidth * 2 - titleUrlPaddingEnd;

        when(titleUrlContainer.getWidth()).thenReturn(threshold + 10);
        when(titleUrlContainer.getLayoutParams())
                .thenReturn(
                        new FrameLayout.LayoutParams(
                                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        mToolbar.onMeasure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);

        when(titleUrlContainer.getWidth()).thenReturn(threshold - 10);
        mToolbar.onMeasure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        assertEquals("Maximize button should be hidden", View.GONE, maximizeButton.getVisibility());

        mToolbar.removeSideSheetMaximizeButton();
        assertEquals("Maximize button should be hidden", View.GONE, maximizeButton.getVisibility());

        mToolbar.removeSideSheetMaximizeButton();
        assertFalse(mToolbar.isMaximizeButtonEnabledForTesting());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_MINIMIZED})
    public void testMinimizeButtonEnabled() {
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<Activity>(mActivity));
        MinimizedFeatureUtils.setMinimizeCustomTabAvailableForTesting(true);
        setup();
        LinearLayout closeMinimizeLayout = mToolbar.findViewById(R.id.close_minimize_layout);
        var minimizeButton = (ImageButton) mToolbar.findViewById(R.id.custom_tabs_minimize_button);
        View titleUrlContainer = Mockito.mock(View.class);
        when(titleUrlContainer.getLayoutParams())
                .thenReturn(
                        new FrameLayout.LayoutParams(
                                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        mLocationBar.setTitleUrlContainerForTesting(titleUrlContainer);
        // Button on left side
        assertEquals(
                "Minimize button should be visible", View.VISIBLE, minimizeButton.getVisibility());
        assertEquals(
                "Minimize button should be to the inside of close button",
                closeMinimizeLayout.getChildAt(1),
                minimizeButton);

        // Button on right side
        mToolbar.setCloseButtonPosition(CustomTabsIntent.CLOSE_BUTTON_POSITION_END);
        mToolbar.onMeasure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        assertEquals(
                "Minimize button should be visible", View.VISIBLE, minimizeButton.getVisibility());
        assertEquals(
                "Minimize button should be to the inside of close button",
                closeMinimizeLayout.getChildAt(0),
                minimizeButton);

        // No space for minimize button
        when(titleUrlContainer.getWidth()).thenReturn(60);
        mToolbar.onMeasure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        assertEquals("Minimize button should be hidden", View.GONE, minimizeButton.getVisibility());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_MINIMIZED})
    public void testMinimizeButtonDisabled() {
        LinearLayout closeMinimizeLayout = mToolbar.findViewById(R.id.close_minimize_layout);
        var minimizeButton = (ImageButton) mToolbar.findViewById(R.id.custom_tabs_minimize_button);
        var closeButton = (ImageButton) mToolbar.findViewById(R.id.close_button);

        // Button on left side
        assertNull("Minimize button should never be initialized", minimizeButton);
        assertEquals(
                "Close button should still be present",
                closeMinimizeLayout.getChildAt(0),
                closeButton);

        // Button on right side
        mToolbar.setCloseButtonPosition(CustomTabsIntent.CLOSE_BUTTON_POSITION_END);
        mToolbar.onMeasure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        assertNull("Minimize button should never be initialized", minimizeButton);
        assertEquals(
                "Close button should still be present",
                closeMinimizeLayout.getChildAt(1),
                closeButton);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_MINIMIZED})
    public void testMinimizeButtonEnabled_MultiWindowMode() {
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<Activity>(mActivity));
        MinimizedFeatureUtils.setMinimizeCustomTabAvailableForTesting(true);
        setup();
        // Not in multi-window, show minimize button.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);

        LinearLayout closeMinimizeLayout = mToolbar.findViewById(R.id.close_minimize_layout);
        mToolbar.onMeasure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        assertEquals(
                "Close button should be visible",
                View.VISIBLE,
                closeMinimizeLayout.getChildAt(0).getVisibility());
        assertEquals(
                "Minimize button should be visible",
                View.VISIBLE,
                closeMinimizeLayout.getChildAt(1).getVisibility());

        MinimizedFeatureUtils.setMinimizeCustomTabAvailableForTesting(true);
        setup();
        // In multi-window, hide minimize button visibility.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        closeMinimizeLayout = mToolbar.findViewById(R.id.close_minimize_layout);
        mToolbar.onMeasure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        assertNotNull(closeMinimizeLayout.getChildAt(1));
        assertEquals(
                "Minimize button should NOT be visible",
                View.GONE,
                closeMinimizeLayout.getChildAt(1).getVisibility());
        assertEquals(
                "Close button should be visible",
                View.VISIBLE,
                closeMinimizeLayout.getChildAt(0).getVisibility());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_SIDE_SHEET})
    public void testHandleStrategy_ClickCloseListener() {
        var strategy1 = new SimpleHandleStrategy(r -> {});
        mToolbar.setHandleStrategy(strategy1);

        View.OnClickListener listener = v -> {};
        mToolbar.setCustomTabCloseClickHandler(listener);
        assertNotNull(strategy1.getClickCloseHandlerForTesting());

        var strategy2 = new SimpleHandleStrategy(r -> {});
        // Another call to #setHandleStrategy which can come from device rotation.
        // HandleStrategy should be initialized properly in response.
        mToolbar.setHandleStrategy(strategy2);
        assertNotNull(strategy2.getClickCloseHandlerForTesting());
    }

    @Test
    public void testContainerVisibilityChange() {
        // Self changes should be ignored.
        mToolbar.addContainerVisibilityChangeObserver(mContainerVisibilityChangeObserver);
        mToolbar.onVisibilityChanged(mToolbar, View.VISIBLE);
        verify(mContainerVisibilityChangeObserver, never()).onResult(any());

        mToolbar.onVisibilityChanged(mParentView, View.VISIBLE);
        verify(mContainerVisibilityChangeObserver, times(1)).onResult(View.VISIBLE);

        // After removing, no more events.
        reset(mContainerVisibilityChangeObserver);
        mToolbar.removeContainerVisibilityChangeObserver(mContainerVisibilityChangeObserver);
        mToolbar.onVisibilityChanged(mParentView, View.VISIBLE);
        verify(mContainerVisibilityChangeObserver, never()).onResult(any());
    }

    @Test
    public void testCookieControlsIcon_animateOnPageStoppedLoadingWithHighBreakageConfidence() {
        verify(mAnimationDelegate, never()).updateSecurityButton(anyInt(), anyBoolean());

        mLocationBar.onBreakageConfidenceLevelChanged(CookieControlsBreakageConfidenceLevel.HIGH);
        verify(mAnimationDelegate, never()).updateSecurityButton(anyInt(), anyBoolean());
        verify(mPageInfoIPHController, never()).showCookieControlsIPH(anyInt(), anyInt());

        mLocationBar.onPageLoadStopped();
        verify(mAnimationDelegate, times(1)).updateSecurityButton(R.drawable.ic_eye_crossed, true);
        verify(mPageInfoIPHController, times(1)).showCookieControlsIPH(anyInt(), anyInt());

        mLocationBar.onBreakageConfidenceLevelChanged(CookieControlsBreakageConfidenceLevel.LOW);
        mLocationBar.onPageLoadStopped();
        verify(mAnimationDelegate, times(1)).updateSecurityButton(R.drawable.ic_eye_crossed, true);
        verify(mPageInfoIPHController, times(1)).showCookieControlsIPH(anyInt(), anyInt());
    }

    @Test
    public void testCookieControlsIcon_trackingProtectionsEnabled_cookieBlockingEnabled() {
        verify(mAnimationDelegate, never()).updateSecurityButton(anyInt(), anyBoolean());

        mLocationBar.onBreakageConfidenceLevelChanged(CookieControlsBreakageConfidenceLevel.HIGH);
        mLocationBar.onStatusChanged(
                CookieControlsStatus.ENABLED,
                /* enforcement= */ 0,
                CookieBlocking3pcdStatus.LIMITED,
                /* expiration= */ 0);

        // Should show only the reminder IPH.
        mLocationBar.onPageLoadStopped();
        verify(mPageInfoIPHController, never()).showCookieControlsIPH(anyInt(), anyInt());
        verify(mPageInfoIPHController, times(1)).showCookieControlsReminderIPH(anyInt(), anyInt());
    }

    @Test
    public void testCookieControlsIcon_trackingProtectionsEnabled_cookieBlockingDisabled() {
        verify(mAnimationDelegate, never()).updateSecurityButton(anyInt(), anyBoolean());

        mLocationBar.onBreakageConfidenceLevelChanged(CookieControlsBreakageConfidenceLevel.HIGH);
        mLocationBar.onStatusChanged(
                CookieControlsStatus.DISABLED,
                /* enforcement= */ 0,
                CookieBlocking3pcdStatus.LIMITED,
                /* expiration= */ 0);

        // None of the IPHs should be shown.
        mLocationBar.onPageLoadStopped();
        verify(mPageInfoIPHController, never()).showCookieControlsIPH(anyInt(), anyInt());
        verify(mPageInfoIPHController, never()).showCookieControlsReminderIPH(anyInt(), anyInt());
    }

    @Test
    public void testCookieControlsIcon_trackingProtectionDisabled_cookieBlockingEnabled() {
        verify(mAnimationDelegate, never()).updateSecurityButton(anyInt(), anyBoolean());

        mLocationBar.onBreakageConfidenceLevelChanged(CookieControlsBreakageConfidenceLevel.HIGH);
        mLocationBar.onStatusChanged(
                CookieControlsStatus.ENABLED,
                /* enforcement= */ 0,
                CookieBlocking3pcdStatus.NOT_IN3PCD,
                /* expiration= */ 0);

        // Should show only the Cookie controls IPH.
        mLocationBar.onPageLoadStopped();
        verify(mPageInfoIPHController, times(1)).showCookieControlsIPH(anyInt(), anyInt());
        verify(mPageInfoIPHController, never()).showCookieControlsReminderIPH(anyInt(), anyInt());
    }

    private void assertUrlAndTitleVisible(boolean titleVisible, boolean urlVisible) {
        int expectedTitleVisibility = titleVisible ? View.VISIBLE : View.GONE;
        int expectedUrlVisibility = urlVisible ? View.VISIBLE : View.GONE;

        assertEquals(
                "Title visibility is off.", expectedTitleVisibility, mTitleBar.getVisibility());
        assertEquals("URL bar visibility is off.", expectedUrlVisibility, mUrlBar.getVisibility());
    }

    private void assertUrlBarShowingText(String expectedString) {
        assertEquals("URL bar is not visible.", mUrlBar.getVisibility(), View.VISIBLE);
        assertEquals("URL bar text does not match.", expectedString, mUrlBar.getText().toString());
    }

    private void assertBrandingTextShowingOnUrlBar() {
        assertUrlBarShowingText(mActivity.getResources().getString(R.string.twa_running_in_chrome));
    }

    private void verifyBrowserControlVisibleForRequiredDuration() {
        // Verify browser control is visible for required duration (3000ms).
        ShadowLooper looper = Shadows.shadowOf(Looper.getMainLooper());
        verify(mControlsVisibleDelegate).showControlsPersistent();
        looper.idleFor(2999, TimeUnit.MILLISECONDS);
        verify(mControlsVisibleDelegate, never()).releasePersistentShowingToken(anyInt());
        looper.idleFor(1, TimeUnit.MILLISECONDS);
        verify(mControlsVisibleDelegate).releasePersistentShowingToken(anyInt());
    }

    private void fakeTextureCapture() {
        mToolbar.setTextureCaptureMode(true);
        // Normally an actual capture would happen here.
        mToolbar.setTextureCaptureMode(false);
    }

    private void setUpForAboutBlank() {
        UrlBarData urlBarData = UrlBarData.forUrl(JUnitTestGURLs.ABOUT_BLANK);
        when(mLocationBarModel.getUrlBarData()).thenReturn(urlBarData);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.ABOUT_BLANK);
    }

    private void setUpForUrl(GURL url) {
        Mockito.doReturn(url).when(mTab).getUrl();
        Mockito.doReturn(UrlBarData.forUrl(url)).when(mLocationBarModel).getUrlBarData();
    }
}
