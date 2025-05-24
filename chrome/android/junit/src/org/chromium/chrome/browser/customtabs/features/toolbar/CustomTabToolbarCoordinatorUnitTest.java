// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.SHARE_CUSTOM_ACTIONS_IN_CCT;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Build;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.customtabs.CloseButtonVisibilityManager;
import org.chromium.chrome.browser.customtabs.CustomButtonParamsImpl;
import org.chromium.chrome.browser.customtabs.CustomTabCompositorContentInitializer;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegateSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.util.TokenHolder;
import org.chromium.url.GURL;

/** Tests for {@link CustomTabToolbarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({SHARE_CUSTOM_ACTIONS_IN_CCT})
public class CustomTabToolbarCoordinatorUnitTest {
    private static final int SCREEN_WIDTH = 800;
    private static final int SCREEN_HEIGHT = 1600;
    private static final int SYS_APP_HEADER_HEIGHT = 40;
    private static final int LEFT_INSET = 50;
    private static final int RIGHT_INSET = 60;
    private static final Rect WINDOW_RECT = new Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    private static final Rect WIDEST_UNOCCLUDED_RECT =
            new Rect(LEFT_INSET, 0, SCREEN_WIDTH - RIGHT_INSET, SYS_APP_HEADER_HEIGHT);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    @Mock private ShareDelegate mShareDelegate;
    @Mock private ShareDelegateSupplier mShareDelegateSupplier;
    @Mock private CustomTabActivityTabProvider mTabProvider;
    @Mock private ActivityWindowAndroid mActivityWindowAndroid;
    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Mock private CloseButtonVisibilityManager mCloseButtonVisibilityManager;
    @Mock private CustomTabBrowserControlsVisibilityDelegate mVisibilityDelegate;
    @Mock private CustomTabCompositorContentInitializer mCompositorContentInitializer;
    @Mock private CustomTabToolbarColorController mToolbarColorController;
    @Mock private Tab mTab;
    @Mock private CustomButtonParams mCustomButtonParams;
    @Mock private PendingIntent mPendingIntent;
    @Mock private Activity mActivity;
    @Mock private ToolbarManager mToolbarManager;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;

    private Activity mActivityForResources;
    private CustomTabActivityTabController mTabController;
    private CustomTabToolbarCoordinator mCoordinator;

    @Before
    public void setup() {
        mActivityForResources = Robolectric.setupActivity(Activity.class);
        mTabController = env.createTabController();
        mCoordinator = createCoordinator();

        ShareDelegateSupplier.setInstanceForTesting(mShareDelegateSupplier);
        when(mShareDelegateSupplier.get()).thenReturn(mShareDelegate);
        when(mTabProvider.getTab()).thenReturn(mTab);
        when(mTab.getOriginalUrl()).thenReturn(GURL.emptyGURL());
        when(mTab.getTitle()).thenReturn("");
        when(mCustomButtonParams.getDescription()).thenReturn("");
        when(mCustomButtonParams.getPendingIntent()).thenReturn(mPendingIntent);
    }

    private CustomTabToolbarCoordinator createCoordinator() {
        return new CustomTabToolbarCoordinator(
                env.intentDataProvider,
                mTabProvider,
                mActivity,
                mActivityWindowAndroid,
                mBrowserControlsVisibilityManager,
                env.createNavigationController(mTabController),
                mCloseButtonVisibilityManager,
                mVisibilityDelegate,
                mToolbarColorController,
                mDesktopWindowStateManager,
                mCompositorContentInitializer);
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    private void clickButtonAndVerifyPendingIntent() {
        try {
            mCoordinator.onCustomButtonClick(mCustomButtonParams);
            verify(mShareDelegate, never()).share(any(Tab.class), eq(false), anyInt());
            verify(mPendingIntent)
                    .send(any(), eq(0), any(Intent.class), any(), isNull(), isNull(), any());
        } catch (PendingIntent.CanceledException e) {
            assert false;
        }
    }

    @Test
    public void testCreateShareButtonWithCustomActions() {
        int testColor = 0x99aabbcc;
        mCoordinator.onCustomButtonClick(
                CustomButtonParamsImpl.createShareButton(mActivityForResources, testColor));
        verify(mShareDelegate)
                .share(any(), eq(false), eq(ShareDelegate.ShareOrigin.CUSTOM_TAB_SHARE_BUTTON));
    }

    @Test
    public void testCustomButtonClicked() {
        when(mCustomButtonParams.getType()).thenReturn(CustomButtonParams.ButtonType.OTHER);
        clickButtonAndVerifyPendingIntent();
    }

    @Test
    public void testNullSupplierShareButtonClick() {
        when(mCustomButtonParams.getType())
                .thenReturn(CustomButtonParams.ButtonType.CCT_SHARE_BUTTON);

        // Test null supplier.
        when(mShareDelegateSupplier.get()).thenReturn(null);
        clickButtonAndVerifyPendingIntent();
    }

    @Test
    @DisableFeatures({SHARE_CUSTOM_ACTIONS_IN_CCT})
    public void testShareWithoutCustomActions() {
        when(mCustomButtonParams.getType())
                .thenReturn(CustomButtonParams.ButtonType.CCT_SHARE_BUTTON);

        clickButtonAndVerifyPendingIntent();
    }

    @Test
    public void testToolbarInitialized_closeButtonEnabled() {
        when(env.intentDataProvider.isCloseButtonEnabled()).thenReturn(true);

        mCoordinator.onToolbarInitialized(mToolbarManager);
        verify(mCloseButtonVisibilityManager).setVisibility(true);
    }

    @Test
    public void testToolbarInitialized_closeButtonDisabled() {
        when(env.intentDataProvider.isCloseButtonEnabled()).thenReturn(false);

        mCoordinator.onToolbarInitialized(mToolbarManager);
        verify(mCloseButtonVisibilityManager).setVisibility(false);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @EnableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
    public void testWebAppEnterDW_HideMenuButton() {
        // Setup web app in fullscreen mode.
        when(env.intentDataProvider.getActivityType())
                .thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        when(env.intentDataProvider.getResolvedDisplayMode()).thenReturn(DisplayMode.MINIMAL_UI);
        var appHeaderState = new AppHeaderState(WINDOW_RECT, WIDEST_UNOCCLUDED_RECT, false);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);
        mCoordinator = createCoordinator();

        // Verify menu button and custom actions are visible.
        mCoordinator.onToolbarInitialized(mToolbarManager);
        verify(mToolbarManager).releaseHideMenuButtonToken(TokenHolder.INVALID_TOKEN);
        verify(mToolbarManager).setCustomActionsVisibility(true);

        // Enter desktop windowing.
        when(mToolbarManager.hideMenuButtonPersistently(TokenHolder.INVALID_TOKEN)).thenReturn(0);
        appHeaderState = new AppHeaderState(WINDOW_RECT, WIDEST_UNOCCLUDED_RECT, true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);

        // Verify menu button is hidden.
        var observer = mCoordinator.getAppHeaderObserver();
        assertNotNull("Observer should be initialized", observer);

        // Verify menu button and custom actions are hidden.
        observer.onDesktopWindowingModeChanged(true);
        verify(mToolbarManager).hideMenuButtonPersistently(TokenHolder.INVALID_TOKEN);
        verify(mToolbarManager).setCustomActionsVisibility(false);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @EnableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
    public void testWebAppExitDW_ShowMenuButton() {
        // Setup web app in desktop windowing mode.
        when(env.intentDataProvider.getActivityType())
                .thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        when(env.intentDataProvider.getResolvedDisplayMode()).thenReturn(DisplayMode.MINIMAL_UI);
        var appHeaderState = new AppHeaderState(WINDOW_RECT, WIDEST_UNOCCLUDED_RECT, true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);
        when(mToolbarManager.hideMenuButtonPersistently(TokenHolder.INVALID_TOKEN)).thenReturn(0);
        mCoordinator = createCoordinator();

        // Verify menu button and custom actions are hidden.
        mCoordinator.onToolbarInitialized(mToolbarManager);
        verify(mToolbarManager).hideMenuButtonPersistently(TokenHolder.INVALID_TOKEN);
        verify(mToolbarManager).setCustomActionsVisibility(false);

        // Exit desktop windowing.
        appHeaderState = new AppHeaderState(WINDOW_RECT, WIDEST_UNOCCLUDED_RECT, false);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);

        // Verify menu button is shown.
        var observer = mCoordinator.getAppHeaderObserver();
        assertNotNull("Observer should be initialized", observer);

        observer.onDesktopWindowingModeChanged(false);
        verify(mToolbarManager).releaseHideMenuButtonToken(0);
        verify(mToolbarManager).setCustomActionsVisibility(true);
    }
}
