// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static androidx.browser.customtabs.CustomTabsIntent.CLOSE_BUTTON_POSITION_DEFAULT;

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
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ActivityType.CUSTOM_TAB;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.PRICE_INSIGHTS;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.READER_MODE;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.TRANSLATE;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.UNKNOWN;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Looper;
import android.view.ActionMode;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.browser.customtabs.CustomTabsIntent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
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
import org.chromium.base.FeatureOverrides;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams.ButtonType;
import org.chromium.chrome.browser.customtabs.CustomButtonParamsImpl;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar.CustomTabLocationBar;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.status.PageInfoIphController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup.HistoryDelegate;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.ToolbarSnapshotDifference;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuObserver;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.content_settings.CookieBlocking3pcdStatus;
import org.chromium.components.content_settings.CookieControlsState;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.function.BooleanSupplier;

/** Tests AMP url handling in the CustomTab Toolbar. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowLooper.class, ShadowPostTask.class})
@LooperMode(Mode.PAUSED)
@DisableFeatures(ChromeFeatureList.CCT_TOOLBAR_REFACTOR)
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
    @Mock BrowserStateBrowserControlsVisibilityDelegate mControlsVisibleDelegate;
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
    @Mock AppMenuHandler mAppMenuHandler;
    private @Mock PageInfoIphController mPageInfoIphController;
    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Captor ArgumentCaptor<AppMenuObserver> mAppMenuObserverCaptor;

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
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        setUpForUrl(TEST_URL);
        MinimizedFeatureUtils.setDeviceEligibleForMinimizedCustomTabForTesting(true);
        when(mIntentDataProvider.getCustomTabMode())
                .thenReturn(BrowserServicesIntentDataProvider.CustomTabProfileType.REGULAR);
        when(mIntentDataProvider.getCloseButtonPosition())
                .thenReturn(CLOSE_BUTTON_POSITION_DEFAULT);
        when(mIntentDataProvider.isCloseButtonEnabled()).thenReturn(true);
        when(mIntentDataProvider.getActivityType()).thenReturn(CUSTOM_TAB);
        when(mIntentDataProvider.isOptionalButtonSupported())
                .thenReturn(ChromeFeatureList.sCctAdaptiveButton.isEnabled());

        mActivity = Robolectric.buildActivity(TestActivity.class).get();
        var shareButtonParams = CustomButtonParamsImpl.createShareButton(mActivity, Color.WHITE);
        var actionButtons = List.of(shareButtonParams);
        when(mIntentDataProvider.getCustomButtonsOnToolbar()).thenReturn(actionButtons);
        int toolbarLayout =
                ChromeFeatureList.sCctToolbarRefactor.isEnabled()
                        ? R.layout.new_custom_tab_toolbar
                        : R.layout.custom_tabs_toolbar;
        mToolbar =
                (CustomTabToolbar)
                        LayoutInflater.from(mActivity).inflate(toolbarLayout, null, false);
        ObservableSupplierImpl<Tracker> trackerSupplier = new ObservableSupplierImpl<>();
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
                /* homeButtonDisplay= */ null,
                null,
                mThemeColorProvider,
                mIncognitoStateProvider,
                /* incognitoWindowCountSupplier= */ null);
        if (!ChromeFeatureList.sCctToolbarRefactor.isEnabled()) {
            mToolbar.initVisibilityRule(mActivity, () -> mAppMenuHandler, mIntentDataProvider);
        }
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
        ShadowLooper.idleMainLooper();
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
        ShadowLooper.idleMainLooper();
        mLocationBar.onUrlChanged(false);
        assertEquals("The title should be gone.", View.GONE, mTitleBar.getVisibility());
    }

    @Test
    public void testCannotHideUrlForAboutBlank() {
        setUpForAboutBlank();
        mLocationBar.setUrlBarHidden(true);
        ShadowLooper.idleMainLooper();
        mLocationBar.onUrlChanged(false);
        assertEquals("The url bar should be visible.", View.VISIBLE, mUrlBar.getVisibility());
        assertEquals(
                "The url bar should show about:blank",
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL,
                mUrlBar.getText().toString());
    }

    @Test
    public void testMaximizeButton() {
        assertFalse(mToolbar.isMaximizeButtonEnabledForTesting());
        mToolbar.initSideSheetMaximizeButton(/* maximizedOnInit= */ false, () -> true);
        assertTrue(mToolbar.isMaximizeButtonEnabledForTesting());
        var maximizeButton = mToolbar.findViewById(R.id.custom_tabs_sidepanel_maximize);

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
        mToolbar.setToolbarWidthForTesting(48 * 2 + 68);
        assertEquals("Maximize button should be hidden", View.GONE, maximizeButton.getVisibility());

        mToolbar.removeSideSheetMaximizeButton();
        assertEquals("Maximize button should be hidden", View.GONE, maximizeButton.getVisibility());

        mToolbar.removeSideSheetMaximizeButton();
        assertFalse(mToolbar.isMaximizeButtonEnabledForTesting());
    }

    @Test
    public void testMinimizeButtonEnabled() {
        MinimizedFeatureUtils.setDeviceEligibleForMinimizedCustomTabForTesting(true);
        setup();
        ImageButton minimizeButton = mToolbar.findViewById(R.id.custom_tabs_minimize_button);
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
                "Minimize button should be on the left side of the toolbar",
                mToolbar.getChildAt(1),
                minimizeButton);

        // Button on right side
        mToolbar.setCloseButtonPosition(CustomTabsIntent.CLOSE_BUTTON_POSITION_END);
        mToolbar.onMeasure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        assertEquals(
                "Minimize button should be visible", View.VISIBLE, minimizeButton.getVisibility());
        assertEquals(
                "Minimize button should still be on the left side of the toolbar",
                mToolbar.getChildAt(1),
                minimizeButton);

        // No space for minimize button
        when(titleUrlContainer.getWidth()).thenReturn(60);
        mToolbar.onMeasure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        mToolbar.setToolbarWidthForTesting(48 + 68);
        assertEquals("Minimize button should be hidden", View.GONE, minimizeButton.getVisibility());
    }

    @Test
    public void testMinimizeButtonEnabled_MultiWindowMode() {
        MinimizedFeatureUtils.setDeviceEligibleForMinimizedCustomTabForTesting(true);
        setup();
        // Not in multi-window, show minimize button.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);
        ImageButton minimizeButton = mToolbar.findViewById(R.id.custom_tabs_minimize_button);
        ImageButton closeButton = mToolbar.findViewById(R.id.close_button);

        mToolbar.onMeasure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        assertEquals("Close button should be visible", View.VISIBLE, closeButton.getVisibility());
        assertEquals(
                "Minimize button should be visible", View.VISIBLE, minimizeButton.getVisibility());

        MinimizedFeatureUtils.setDeviceEligibleForMinimizedCustomTabForTesting(true);
        setup();
        minimizeButton = mToolbar.findViewById(R.id.custom_tabs_minimize_button);
        closeButton = mToolbar.findViewById(R.id.close_button);
        // In multi-window, hide minimize button visibility.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        mToolbar.onMeasure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        assertEquals(
                "Minimize button should NOT be visible", View.GONE, minimizeButton.getVisibility());
        assertEquals("Close button should be visible", View.VISIBLE, closeButton.getVisibility());
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
    public void
            testCookieControlsIcon_trackingProtectionsEnabled_cookieBlockingDisabled_doesNotDisplayIph() {
        verify(mAnimationDelegate, never()).updateSecurityButton(anyInt());

        mLocationBar.onHighlightCookieControl(true);
        mLocationBar.onStatusChanged(
                CookieControlsState.HIDDEN,
                /* enforcement= */ 0,
                CookieBlocking3pcdStatus.LIMITED,
                /* expiration= */ 0);

        // None of the IPHs should be shown.
        mLocationBar.onPageLoadStopped();
        verify(mPageInfoIphController, never()).showCookieControlsIph(anyInt(), anyInt());
    }

    @Test
    public void
            testCookieControlsIcon_trackingProtectionDisabled_cookieBlockingEnabled_displaysCookieControlsIph() {
        verify(mAnimationDelegate, never()).updateSecurityButton(anyInt());

        mLocationBar.onHighlightCookieControl(true);
        mLocationBar.onStatusChanged(
                CookieControlsState.BLOCKED3PC,
                /* enforcement= */ 0,
                CookieBlocking3pcdStatus.NOT_IN3PCD,
                /* expiration= */ 0);

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
    @EnableFeatures({ChromeFeatureList.CCT_TOOLBAR_REFACTOR})
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

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testOptionalButton_notEnabledForSearchInCct() {
        var connection = spy(CustomTabsConnection.getInstance());
        Mockito.doReturn(true).when(connection).shouldEnableOmniboxForIntent(any());
        CustomTabsConnection.setInstanceForTesting(connection);
        mToolbar.updateOptionalButton(getDataForPriceInsightsIconButton());
        assertNull(mToolbar.getOptionalButtonCoordinatorForTesting());
        assertEquals(View.GONE, mToolbar.findViewById(R.id.menu_dot).getVisibility());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testOptionalButton_notEnabledForMultipleDevButtons() {
        mToolbar.addCustomActionButton(
                AppCompatResources.getDrawable(mActivity, R.drawable.ic_share_white_24dp),
                "share",
                mock(OnClickListener.class),
                ButtonType.CCT_SHARE_BUTTON);
        mToolbar.addCustomActionButton(
                AppCompatResources.getDrawable(mActivity, R.drawable.ic_book_round),
                "bookmark",
                mock(OnClickListener.class),
                ButtonType.OTHER);
        mToolbar.updateOptionalButton(getDataForPriceInsightsIconButton());
        assertNull(mToolbar.getOptionalButtonCoordinatorForTesting());
        assertEquals(View.VISIBLE, mToolbar.findViewById(R.id.menu_dot).getVisibility());
        verify(mAppMenuHandler).addObserver(mAppMenuObserverCaptor.capture());

        // Verify that the corresponding menu item gets highlighted.
        verify(mAppMenuHandler).setMenuHighlight(eq(R.id.price_insights_menu_id), eq(false));

        // Verify the menu dot disappears as the overflow menu show up.
        mAppMenuObserverCaptor.getValue().onMenuVisibilityChanged(/* isVisible= */ true);
        assertEquals(View.GONE, mToolbar.findViewById(R.id.menu_dot).getVisibility());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testOptionalButton_readerMode_notEnabledForWidthConstraint() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.CCT_ADAPTIVE_BUTTON,
                ReaderModeManager.CPA_FALLBACK_MENU_PARAM,
                true);
        testOptionalButton_notEnabledForWidthConstraint(
                READER_MODE, getDataForReaderModeIconButton());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testOptionalButton_priceInsights_notEnabledForWidthConstraint() {
        testOptionalButton_notEnabledForWidthConstraint(
                PRICE_INSIGHTS, getDataForPriceInsightsIconButton());
    }

    private void testOptionalButton_notEnabledForWidthConstraint(
            @AdaptiveToolbarButtonVariant int variant, ButtonData buttonData) {
        int urlBarWidth =
                mActivity.getResources().getDimensionPixelSize(R.dimen.location_bar_min_url_width);
        int buttonWidth =
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
        // Set the toolbar width small enough (just a single button and the url bar will fit) to
        // have MTB hidden.
        mToolbar.setToolbarWidthForTesting(urlBarWidth + buttonWidth);
        mToolbar.updateOptionalButton(buttonData);

        // For MTB hidden due to width constraint, |OptionButtonCoordinator| is instantiated
        // since the button visibility rule needs to be applied after the MTB is added to
        // the toolbar. If toolbar width changes dynamically later, it lets the optional button
        // start showing.
        assertNotNull(mToolbar.getOptionalButtonCoordinatorForTesting());
        assertEquals(View.VISIBLE, mToolbar.findViewById(R.id.menu_dot).getVisibility());
        assertEquals(
                "Fallback UI should be set",
                variant,
                mToolbar.getVariantForFallbackMenuForTesting());

        // Tapping non-fallback menu item like 'Translate...' has no effect.
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("CustomTab.AdaptiveToolbarButton.FallbackUi")
                        .build();
        mToolbar.maybeRecordHistogramForAdaptiveToolbarButtonFallbackUi(TRANSLATE);
        watcher.assertExpected();

        // Tapping the matching menu item leads to logging the histogram.
        watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTab.AdaptiveToolbarButton.FallbackUi", variant);
        mToolbar.maybeRecordHistogramForAdaptiveToolbarButtonFallbackUi(variant);
        watcher.assertExpected();
        assertEquals(
                "Fallback UI should be reset",
                UNKNOWN,
                mToolbar.getVariantForFallbackMenuForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testOptionalButton_resetsOptionalButtonState() {
        int urlBarWidth =
                mActivity.getResources().getDimensionPixelSize(R.dimen.location_bar_min_url_width);
        int buttonWidth =
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_button_width);
        // Set the toolbar width small enough (just a single button and the url bar will fit) to
        // have MTB hidden.
        mToolbar.setToolbarWidthForTesting(urlBarWidth + buttonWidth);
        mToolbar.updateOptionalButton(getDataForPriceInsightsIconButton());

        assertNotNull(mToolbar.getOptionalButtonCoordinatorForTesting());
        assertEquals(View.VISIBLE, mToolbar.findViewById(R.id.menu_dot).getVisibility());
        assertEquals(
                "Fallback UI should be set",
                PRICE_INSIGHTS,
                mToolbar.getVariantForFallbackMenuForTesting());
        mToolbar.resetOptionalButtonState();
        assertEquals(
                "Fallback UI should be reset",
                UNKNOWN,
                mToolbar.getVariantForFallbackMenuForTesting());
        assertEquals(View.GONE, mToolbar.findViewById(R.id.menu_dot).getVisibility());
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

    private ButtonData getDataForPriceInsightsIconButton() {
        Drawable iconDrawable =
                AppCompatResources.getDrawable(mActivity, R.drawable.ic_trending_down_24dp);
        OnClickListener clickListener = mock(OnClickListener.class);
        OnLongClickListener longClickListener = mock(OnLongClickListener.class);
        String contentDescription = mActivity.getString(R.string.price_insights_title);

        // Whether a button is static or dynamic is determined by the button variant.
        ButtonSpec buttonSpec =
                new ButtonSpec(
                        iconDrawable,
                        clickListener,
                        longClickListener,
                        contentDescription,
                        true,
                        null,
                        /* buttonVariant= */ AdaptiveToolbarButtonVariant.PRICE_INSIGHTS,
                        /* actionChipLabelResId= */ Resources.ID_NULL,
                        /* tooltipTextResId= */ Resources.ID_NULL,
                        /* hasErrorBadge= */ false);
        ButtonDataImpl buttonData = new ButtonDataImpl();
        buttonData.setButtonSpec(buttonSpec);
        buttonData.setCanShow(true);
        buttonData.setEnabled(true);
        return buttonData;
    }

    private ButtonData getDataForReaderModeIconButton() {
        Drawable iconDrawable =
                AppCompatResources.getDrawable(mActivity, R.drawable.ic_mobile_friendly_24dp);
        OnClickListener clickListener = mock(OnClickListener.class);
        OnLongClickListener longClickListener = mock(OnLongClickListener.class);
        String contentDescription = mActivity.getString(R.string.reader_mode_cpa_button_text);

        // Whether a button is static or dynamic is determined by the button variant.
        ButtonSpec buttonSpec =
                new ButtonSpec(
                        iconDrawable,
                        clickListener,
                        longClickListener,
                        contentDescription,
                        true,
                        null,
                        /* buttonVariant= */ READER_MODE,
                        /* actionChipLabelResId= */ Resources.ID_NULL,
                        /* tooltipTextResId= */ Resources.ID_NULL,
                        /* hasErrorBadge= */ false);
        ButtonDataImpl buttonData = new ButtonDataImpl();
        buttonData.setButtonSpec(buttonSpec);
        buttonData.setCanShow(true);
        buttonData.setEnabled(true);
        return buttonData;
    }
}
