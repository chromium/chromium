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
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.os.Looper;
import android.view.ActionMode;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

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
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar.CustomTabLocationBar;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.status.PageInfoIphController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup.HistoryDelegate;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.ToolbarSnapshotDifference;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.content_settings.CookieControlsState;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.security_state.ConnectionSecurityLevel;
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
        shadows = {ShadowLooper.class})
public class CustomTabToolbarUnitTest {
    private static final GURL TEST_URL = JUnitTestGURLs.INITIAL_URL;
    private static final GURL AMP_URL =
            new GURL("https://www.google.com/amp/www.nyt.com/ampthml/blogs.html");
    private static final GURL AMP_CACHE_URL =
            new GURL("https://www.google.com/amp/s/www.nyt.com/ampthml/blogs.html");

    @Rule public MockitoRule mRule = MockitoJUnit.rule();

    @Mock LocationBarModel mLocationBarModel;
    @Mock ActionMode.Callback mActionModeCallback;
    @Mock CustomTabToolbarAnimationDelegate mAnimationDelegate;
    @Mock ToolbarDataProvider mToolbarDataProvider;
    @Mock ToolbarTabController mTabController;
    @Mock MenuButtonCoordinator mMenuButtonCoordinator;
    @Mock private ToggleTabStackButtonCoordinator mTabSwitcherButtonCoordinator;
    @Mock HistoryDelegate mHistoryDelegate;
    @Mock BooleanSupplier mPartnerHomepageEnabledSupplier;
    @Mock UserEducationHelper mUserEducationHelper;
    @Mock Tracker mTracker;
    @Mock Tab mTab;
    @Mock Callback<Integer> mContainerVisibilityChangeObserver;
    @Mock View mParentView;
    @Mock WindowAndroid mWindowAndroid;
    private @Mock PageInfoIphController mPageInfoIphController;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;

    private BrowserStateBrowserControlsVisibilityDelegate mControlsVisibleDelegate;
    private Activity mActivity;
    private CustomTabToolbar mToolbar;
    private CustomTabLocationBar mLocationBar;
    private TextView mTitleBar;
    private TextView mUrlBar;
    private ImageButton mSecurityButton;
    private ImageButton mSecurityIcon;
    private ToolbarProgressBar mToolbarProgressBar;

    @Before
    public void setup() {
        Mockito.doReturn(R.string.accessibility_security_btn_secure)
                .when(mLocationBarModel)
                .getSecurityIconContentDescriptionResourceId();
        Mockito.doReturn(R.color.default_icon_color_tint_list)
                .when(mLocationBarModel)
                .getSecurityIconColorStateList();
        when(mToolbarDataProvider.getTab()).thenReturn(mTab);
        when(mTab.getUserDataHost()).thenReturn(new UserDataHost());
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        setUpForUrl(TEST_URL);

        mControlsVisibleDelegate =
                new BrowserStateBrowserControlsVisibilityDelegate(
                        ObservableSuppliers.alwaysFalse());

        MinimizedFeatureUtils.setDeviceEligibleForMinimizedCustomTabForTesting(true);

        mActivity = Robolectric.buildActivity(TestActivity.class).get();
        mToolbar =
                (CustomTabToolbar)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.new_custom_tab_toolbar, null, false);
        SettableMonotonicObservableSupplier<Tracker> trackerSupplier =
                ObservableSuppliers.createMonotonic();
        trackerSupplier.set(mTracker);
        mToolbarProgressBar = new ToolbarProgressBar(mActivity, null);
        mToolbar.initialize(
                mToolbarDataProvider,
                mTabController,
                mMenuButtonCoordinator,
                mTabSwitcherButtonCoordinator,
                mHistoryDelegate,
                mUserEducationHelper,
                trackerSupplier,
                mToolbarProgressBar,
                null,
                null,
                null,
                /* homeButtonCoordinator= */ null,
                /* signinButtonCoordinator= */ null,
                mThemeColorProvider,
                mIncognitoStateProvider,
                /* incognitoWindowCountSupplier= */ null);
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
        mLocationBar.setIphControllerForTesting(mPageInfoIphController);
        mSecurityButton = mToolbar.findViewById(R.id.security_button);
        mSecurityIcon = mToolbar.findViewById(R.id.security_icon);
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
        mLocationBar.onUrlChanged(false);
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
        if (ChromeFeatureList.sCctNestedSecurityIcon.isEnabled()) return;

        assertUrlAndTitleVisible(/* titleVisible= */ false, /* urlVisible= */ true);
        mLocationBar.showEmptyLocationBar();
        assertUrlAndTitleVisible(/* titleVisible= */ false, /* urlVisible= */ false);

        // Attempt to update title and URL, should noop since location bar is still in empty state.
        mLocationBar.setShowTitle(true);
        mLocationBar.setUrlBarHidden(false);
        verify(mLocationBarModel, never()).notifySecurityStateChanged();

        mLocationBar.showBrandingLocationBar();
        assertUrlAndTitleVisible(/* titleVisible= */ false, /* urlVisible= */ true);
        verify(mAnimationDelegate).updateSecurityButton(anyInt());
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
    public void testIsReadyForTextureCapture() {
        CaptureReadinessResult result = mToolbar.isReadyForTextureCapture();
        assertTrue(result.isReady);
        assertEquals(ToolbarSnapshotDifference.NULL, result.snapshotDifference);

        fakeTextureCapture();
        result = mToolbar.isReadyForTextureCapture();
        assertEquals(ToolbarSnapshotDifference.NONE, result.snapshotDifference);
        assertFalse(result.isReady);

        when(mToolbarDataProvider.getPrimaryColor()).thenReturn(Color.RED);
        mToolbar.onPrimaryColorChanged(false);
        result = mToolbar.isReadyForTextureCapture();
        assertTrue(result.isReady);
        assertEquals(ToolbarSnapshotDifference.TINT, result.snapshotDifference);

        fakeTextureCapture();
        when(mToolbarDataProvider.getTab()).thenReturn(mTab);
        when(mTab.getUserDataHost()).thenReturn(new UserDataHost());
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.RED_1);
        UrlBarData urlBarData = UrlBarData.forUrl(JUnitTestGURLs.RED_1);
        when(mLocationBarModel.getUrlBarData()).thenReturn(urlBarData);
        mLocationBar.onUrlChanged(false);
        result = mToolbar.isReadyForTextureCapture();
        assertTrue(result.isReady);
        assertEquals(ToolbarSnapshotDifference.URL_TEXT, result.snapshotDifference);

        fakeTextureCapture();
        when(mLocationBarModel.hasTab()).thenReturn(true);
        when(mLocationBarModel.getTitle()).thenReturn("Red 1");
        mLocationBar.onTitleChanged();
        result = mToolbar.isReadyForTextureCapture();
        assertTrue(result.isReady);
        assertEquals(ToolbarSnapshotDifference.TITLE_TEXT, result.snapshotDifference);

        fakeTextureCapture();
        when(mLocationBarModel.getSecurityIconResource(anyBoolean()))
                .thenReturn(R.drawable.ic_globe_24dp);
        when(mAnimationDelegate.getSecurityIconRes()).thenReturn(R.drawable.ic_globe_24dp);
        mLocationBar.onSecurityStateChanged();
        result = mToolbar.isReadyForTextureCapture();
        assertTrue(result.isReady);
        assertEquals(ToolbarSnapshotDifference.SECURITY_ICON, result.snapshotDifference);

        fakeTextureCapture();
        when(mAnimationDelegate.isInAnimation()).thenReturn(true);
        result = mToolbar.isReadyForTextureCapture();
        assertTrue(result.isReady);
        assertEquals(ToolbarSnapshotDifference.CCT_ANIMATION, result.snapshotDifference);

        when(mAnimationDelegate.isInAnimation()).thenReturn(false);
        fakeTextureCapture();
        mToolbar.layout(0, 0, 100, 100);
        result = mToolbar.isReadyForTextureCapture();
        assertTrue(result.isReady);
        assertEquals(ToolbarSnapshotDifference.LOCATION_BAR_WIDTH, result.snapshotDifference);
    }

    @Test
    public void testAboutBlankUrlIsShown() {
        setUpForAboutBlank();
        RobolectricUtil.runAllBackgroundAndUi();
        mLocationBar.onUrlChanged(false);
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
        RobolectricUtil.runAllBackgroundAndUi();
        mLocationBar.onUrlChanged(false);
        assertEquals("The title should be gone.", View.GONE, mTitleBar.getVisibility());
    }

    @Test
    public void testCannotHideUrlForAboutBlank() {
        setUpForAboutBlank();
        mLocationBar.setUrlBarHidden(true);
        RobolectricUtil.runAllBackgroundAndUi();
        mLocationBar.onUrlChanged(false);
        assertEquals("The url bar should be visible.", View.VISIBLE, mUrlBar.getVisibility());
        assertEquals(
                "The url bar should show about:blank",
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL,
                mUrlBar.getText().toString());
    }

    @Test
    @SuppressWarnings("unchecked") // reset() is a generic-varargs method.
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
    public void testCookieControlsIcon_onHighlightCookieControl_animateOnPageStoppedLoading() {
        verify(mAnimationDelegate, never()).updateSecurityButton(anyInt());

        mLocationBar.onHighlightCookieControl(true);

        verify(mAnimationDelegate, never()).updateSecurityButton(anyInt());
        verify(mPageInfoIphController, never()).showCookieControlsIph(anyInt(), anyInt());

        mLocationBar.onPageLoadStopped();
        verify(mAnimationDelegate, times(1)).updateSecurityButton(R.drawable.ic_eye_crossed);
        verify(mPageInfoIphController, times(1)).showCookieControlsIph(anyInt(), anyInt());

        mLocationBar.onHighlightCookieControl(false);
        mLocationBar.onPageLoadStopped();
        verify(mAnimationDelegate, times(1)).updateSecurityButton(R.drawable.ic_eye_crossed);
        verify(mPageInfoIphController, times(1)).showCookieControlsIph(anyInt(), anyInt());
    }

    @Test
    public void testCookieControlsIcon_cookieBlockingEnabled_displaysCookieControlsIph() {
        verify(mAnimationDelegate, never()).updateSecurityButton(anyInt());

        mLocationBar.onHighlightCookieControl(true);
        mLocationBar.onStatusChanged(
                CookieControlsState.BLOCKED3PC, /* enforcement= */ 0, /* expiration= */ 0);

        // Should show only the Cookie controls IPH.
        mLocationBar.onPageLoadStopped();
        verify(mPageInfoIphController, times(1)).showCookieControlsIph(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_NESTED_SECURITY_ICON})
    public void testSecurityIconVisibility_nestedIcon() {
        assertEquals(View.GONE, mSecurityButton.getVisibility());
        assertEquals(View.INVISIBLE, mSecurityIcon.getVisibility());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_NESTED_SECURITY_ICON})
    public void testSecurityIconHidden() {
        when(mLocationBarModel.getSecurityIconResource(anyBoolean()))
                .thenReturn(R.drawable.omnibox_https_valid_page_info);
        when(mLocationBarModel.getSecurityLevel()).thenReturn(ConnectionSecurityLevel.SECURE);

        mLocationBar.onSecurityStateChanged();

        verify(mAnimationDelegate).updateSecurityButton(0);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_NESTED_SECURITY_ICON})
    public void testSecurityIconShown() {
        when(mLocationBarModel.getSecurityIconResource(anyBoolean()))
                .thenReturn(R.drawable.omnibox_not_secure_warning);
        when(mLocationBarModel.getSecurityLevel()).thenReturn(ConnectionSecurityLevel.WARNING);

        mLocationBar.onSecurityStateChanged();

        verify(mAnimationDelegate).updateSecurityButton(R.drawable.omnibox_not_secure_warning);
    }

    @Test
    public void testInflatesButtons() {
        assertNull(mToolbar.getMenuButton());
        assertNotNull(mToolbar.ensureMenuButtonInflated());
        assertEquals(View.VISIBLE, mToolbar.getMenuButton().getVisibility());

        assertNull(mToolbar.getCloseButton());
        assertNotNull(mToolbar.ensureCloseButtonInflated());
        assertEquals(View.VISIBLE, mToolbar.getCloseButton().getVisibility());

        assertNull(mToolbar.getMinimizeButton());
        assertNotNull(mToolbar.ensureMinimizeButtonInflated());
        assertEquals(View.VISIBLE, mToolbar.getMinimizeButton().getVisibility());

        assertNull(mToolbar.getSideSheetMaximizeButton());
        assertNotNull(mToolbar.ensureSideSheetMaximizeButtonInflated());
        assertEquals(View.VISIBLE, mToolbar.getSideSheetMaximizeButton().getVisibility());

        var incognitoImageView = mToolbar.ensureIncognitoImageViewInflated();
        assertEquals(View.VISIBLE, incognitoImageView.getVisibility());
    }

    private void assertUrlAndTitleVisible(boolean titleVisible, boolean urlVisible) {
        int expectedTitleVisibility = titleVisible ? View.VISIBLE : View.GONE;
        int expectedUrlVisibility = urlVisible ? View.VISIBLE : View.GONE;

        assertEquals(
                "Title visibility is off.", expectedTitleVisibility, mTitleBar.getVisibility());
        assertEquals("URL bar visibility is off.", expectedUrlVisibility, mUrlBar.getVisibility());
    }

    private void assertUrlBarShowingText(String expectedString) {
        assertEquals("URL bar is not visible.", View.VISIBLE, mUrlBar.getVisibility());
        assertEquals("URL bar text does not match.", expectedString, mUrlBar.getText().toString());
    }

    private void assertBrandingTextShowingOnUrlBar() {
        assertUrlBarShowingText(mActivity.getResources().getString(R.string.twa_running_in_chrome));
    }

    private void verifyBrowserControlVisibleForRequiredDuration() {
        // Verify browser control is visible for required duration (3000ms).
        ShadowLooper looper = Shadows.shadowOf(Looper.getMainLooper());
        assertEquals(
                "Browser controls should be shown.",
                BrowserControlsState.SHOWN,
                mControlsVisibleDelegate.get().intValue());
        looper.idleFor(2999, TimeUnit.MILLISECONDS);
        assertEquals(
                "Browser controls should still be shown.",
                BrowserControlsState.SHOWN,
                mControlsVisibleDelegate.get().intValue());
        looper.idleFor(1, TimeUnit.MILLISECONDS);
        assertEquals(
                "Browser controls should be released.",
                BrowserControlsState.BOTH,
                mControlsVisibleDelegate.get().intValue());
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
