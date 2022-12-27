// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
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
import android.widget.TextView;

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

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.task.test.ShadowPostTask.TestImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar.CustomTabLocationBar;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup.HistoryDelegate;
import org.chromium.chrome.browser.toolbar.top.ToolbarSnapshotState.ToolbarSnapshotDifference;
import org.chromium.chrome.browser.toolbar.top.ToolbarTablet.OfflineDownloader;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.JUnitTestGURLs;

import java.util.concurrent.TimeUnit;
import java.util.function.BooleanSupplier;

/**
 * Tests AMP url handling in the CustomTab Toolbar.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowLooper.class, ShadowPostTask.class})
@LooperMode(Mode.PAUSED)
@EnableFeatures(ChromeFeatureList.CCT_SHOW_ABOUT_BLANK_URL)
@DisableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
public class CustomTabToolbarUnitTest {
    @Rule
    public MockitoRule mRule = MockitoJUnit.rule();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    LocationBarModel mLocationBarModel;
    @Mock
    ActionMode.Callback mActionModeCallback;
    @Mock
    CustomTabToolbarAnimationDelegate mAnimationDelegate;
    @Mock
    BrowserStateBrowserControlsVisibilityDelegate mControlsVisibleDelegate;
    @Mock
    ToolbarDataProvider mToolbarDataProvider;
    @Mock
    ToolbarTabController mTabController;
    @Mock
    MenuButtonCoordinator mMenuButtonCoordinator;
    @Mock
    HistoryDelegate mHistoryDelegate;
    @Mock
    BooleanSupplier mPartnerHomepageEnabledSupplier;
    @Mock
    OfflineDownloader mOfflineDownloader;
    @Mock
    Tab mTab;

    private final ObservableSupplierImpl<Boolean> mIsProgressBarVisibleSupplier =
            new ObservableSupplierImpl<>();

    private Activity mActivity;
    private CustomTabToolbar mToolbar;
    private CustomTabLocationBar mLocationBar;
    private TextView mTitleBar;
    private TextView mUrlBar;

    @Before
    public void setup() {
        ShadowPostTask.setTestImpl(new TestImpl() {
            @Override
            public void postDelayedTask(TaskTraits taskTraits, Runnable task, long delay) {
                new Handler(Looper.getMainLooper()).postDelayed(task, delay);
            }
        });
        Mockito.doReturn(R.string.accessibility_security_btn_secure)
                .when(mLocationBarModel)
                .getSecurityIconContentDescriptionResourceId();
        Mockito.doReturn(R.color.default_icon_color_tint_list)
                .when(mLocationBarModel)
                .getSecurityIconColorStateList();

        mActivity = Robolectric.buildActivity(TestActivity.class).get();
        mToolbar = (CustomTabToolbar) LayoutInflater.from(mActivity).inflate(
                R.layout.custom_tabs_toolbar, null, false);
        mLocationBar = (CustomTabLocationBar) mToolbar.createLocationBar(mLocationBarModel,
                mActionModeCallback, () -> null, () -> null, mControlsVisibleDelegate);
        mUrlBar = mToolbar.findViewById(R.id.url_bar);
        mTitleBar = mToolbar.findViewById(R.id.title_bar);
        mLocationBar.setAnimDelegateForTesting(mAnimationDelegate);

        mToolbar.initialize(mToolbarDataProvider, mTabController, mMenuButtonCoordinator,
                mIsProgressBarVisibleSupplier, mHistoryDelegate, mPartnerHomepageEnabledSupplier,
                mOfflineDownloader);
    }

    @After
    public void tearDown() {
        mActivity.finish();
        ShadowPostTask.reset();
        ChromeFeatureList.sCctBrandTransparency.setForTesting(null);
    }

    @Test
    public void testParsesPublisherFromAmp() {
        assertEquals("www.nyt.com",
                CustomTabToolbar.parsePublisherNameFromUrl(
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.AMP_URL)));
        assertEquals("www.nyt.com",
                CustomTabToolbar.parsePublisherNameFromUrl(
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.AMP_CACHE_URL)));
        assertEquals(JUnitTestGURLs.EXAMPLE_URL,
                CustomTabToolbar.parsePublisherNameFromUrl(
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL)));
    }

    @Test
    public void testShowBranding_DomainOnly() {
        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/true);
        mLocationBar.showBranding();
        ShadowLooper.idleMainLooper();
        verify(mLocationBarModel, never()).notifyTitleChanged();
        verify(mAnimationDelegate, never()).prepareTitleAnim(mUrlBar, mTitleBar);
        verify(mAnimationDelegate, never()).startTitleAnimation(any());
        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/true);

        // Run all UI tasks, until the branding is finished.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLocationBarModel, never()).notifyTitleChanged();
        verify(mLocationBarModel).notifyUrlChanged();
        verify(mLocationBarModel).notifySecurityStateChanged();

        assertEquals("URL bar is not visible.", mUrlBar.getVisibility(), View.VISIBLE);
    }

    @Test
    public void testShowBranding_DomainAndTitle() {
        // Set title before the branding starts, so the state is domain and title.
        mLocationBar.setShowTitle(true);
        ShadowLooper.idleMainLooper();
        verify(mLocationBarModel).notifyTitleChanged();
        verify(mAnimationDelegate).prepareTitleAnim(mUrlBar, mTitleBar);
        // Animation not started since branding is not completed.
        verify(mAnimationDelegate, never()).startTitleAnimation(any());

        // Title should be hidden, title animation is not necessary yet.
        mLocationBar.showBranding();
        ShadowLooper.idleMainLooper();
        verify(mLocationBarModel, times(2)).notifyTitleChanged();
        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/true);

        // Run all UI tasks, until the branding is finished.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLocationBarModel, times(3)).notifyTitleChanged();
        verify(mLocationBarModel).notifyUrlChanged();
        verify(mLocationBarModel).notifySecurityStateChanged();
        verify(mAnimationDelegate, times(2)).prepareTitleAnim(mUrlBar, mTitleBar);

        assertEquals("URL bar is not visible.", mUrlBar.getVisibility(), View.VISIBLE);
    }

    @Test
    public void testToolbarBrandingDelegateImpl_EmptyToRegular() {
        ChromeFeatureList.sCctBrandTransparency.setForTesting(true);

        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/true);
        mLocationBar.showEmptyLocationBar();
        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/false);

        // Attempt to update title and URL, should noop since location bar is still in empty state.
        mLocationBar.onTitleChanged();
        mLocationBar.onUrlChanged();
        verify(mLocationBarModel, never()).notifyUrlChanged();
        verify(mLocationBarModel, never()).notifySecurityStateChanged();

        mLocationBar.showRegularToolbar();
        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/true);
        verify(mLocationBarModel).notifyUrlChanged();
        verify(mLocationBarModel).notifyTitleChanged();
        verify(mLocationBarModel).notifySecurityStateChanged();
        verifyBrowserControlVisibleForRequiredDuration();
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
        ChromeFeatureList.sCctBrandTransparency.setForTesting(true);

        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/true);
        mLocationBar.showEmptyLocationBar();
        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/false);

        // Attempt to update title and URL, should noop since location bar is still in empty state.
        mLocationBar.setShowTitle(true);
        mLocationBar.setUrlBarHidden(false);
        verify(mLocationBarModel, never()).notifyUrlChanged();
        verify(mLocationBarModel, never()).notifySecurityStateChanged();

        mLocationBar.showBrandingLocationBar();
        assertUrlAndTitleVisible(/*titleVisible=*/false, /*urlVisible=*/true);
        verify(mAnimationDelegate).updateSecurityButton(anyInt(), eq(animateIconTransition));

        // Attempt to update title and URL to show Title only - should be ignored during branding.
        reset(mLocationBarModel);
        mLocationBar.setShowTitle(true);
        mLocationBar.setUrlBarHidden(true);
        verifyNoMoreInteractions(mLocationBarModel);

        // After getting back to regular toolbar, title should become visible now.
        mLocationBar.showRegularToolbar();
        assertUrlAndTitleVisible(/*titleVisible=*/true, /*urlVisible=*/false);
        verify(mLocationBarModel, atLeastOnce()).notifyUrlChanged();
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
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1));
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
    }

    @Test
    public void testAboutBlankUrlIsShown() {
        setUpForAboutBlank();
        ShadowLooper.idleMainLooper();
        mLocationBar.onUrlChanged();
        assertEquals("The url bar should be visible.", View.VISIBLE, mUrlBar.getVisibility());
        assertEquals("The url bar should show about:blank",
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL, mUrlBar.getText().toString());
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
        assertEquals("The url bar should show about:blank",
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL, mUrlBar.getText().toString());
    }

    private void assertUrlAndTitleVisible(boolean titleVisible, boolean urlVisible) {
        int expectedTitleVisibility = titleVisible ? View.VISIBLE : View.GONE;
        int expectedUrlVisibility = urlVisible ? View.VISIBLE : View.GONE;

        assertEquals(
                "Title visibility is off.", expectedTitleVisibility, mTitleBar.getVisibility());
        assertEquals("URL bar visibility is off.", expectedUrlVisibility, mUrlBar.getVisibility());
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
        when(mToolbarDataProvider.getTab()).thenReturn(mTab);
        when(mTab.getUserDataHost()).thenReturn(new UserDataHost());
        UrlBarData urlBarData = UrlBarData.forUrl(JUnitTestGURLs.ABOUT_BLANK);
        when(mLocationBarModel.getUrlBarData()).thenReturn(urlBarData);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.ABOUT_BLANK));
    }
}
