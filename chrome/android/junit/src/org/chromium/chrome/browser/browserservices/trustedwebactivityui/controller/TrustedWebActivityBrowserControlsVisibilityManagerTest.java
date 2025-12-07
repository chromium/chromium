// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.view.ContextThemeWrapper;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.browser.customtabs.TrustedWebUtils;
import androidx.browser.trusted.TrustedWebActivityDisplayMode;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CloseButtonVisibilityManager;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;

/** Tests for {@link TrustedWebActivityBrowserControlsVisibilityManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrustedWebActivityBrowserControlsVisibilityManagerTest {
    private static final Rect APP_WINDOW_RECT = new Rect(0, 0, 1600, 800);
    private static final Rect WIDEST_UNOCCLUDED_RECT = new Rect(0, 10, 1580, 760);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock public TabObserverRegistrar mTabObserverRegistrar;
    @Mock public CustomTabActivityTabProvider mTabProvider;
    @Mock public Tab mTab;
    @Mock SecurityStateModel.Natives mSecurityStateMocks;
    @Mock public CustomTabToolbarCoordinator mToolbarCoordinator;
    @Mock public CloseButtonVisibilityManager mCloseButtonVisibilityManager;

    TrustedWebActivityBrowserControlsVisibilityManager mController;

    private @Nullable AppHeaderState mAppHeaderState;
    private Context mContext;

    @Before
    public void setUp() {
        SecurityStateModelJni.setInstanceForTesting(mSecurityStateMocks);
        when(mTabProvider.getTab()).thenReturn(mTab);
        doReturn(Tab.INVALID_TAB_ID).when(mTab).getParentId();
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    /** Browser controls should be shown for pages with certificate errors. */
    @Test
    public void testDangerousSecurityLevel() {
        mController = buildController(mock(BrowserServicesIntentDataProvider.class));
        setTabSecurityLevel(ConnectionSecurityLevel.DANGEROUS);
        mController.updateIsInAppMode(true);
        assertEquals(BrowserControlsState.SHOWN, getLastBrowserControlsState());
        assertFalse(getLastCloseButtonVisibility());
    }

    /** Browser controls should not be shown for WebAPKs with 'minimal-ui' display mode. */
    @Test
    public void testMinimalUiDisplayMode() {
        mController = buildController(buildWebApkIntentDataProvider(DisplayMode.MINIMAL_UI));
        mController.updateIsInAppMode(true);
        assertEquals(BrowserControlsState.HIDDEN, getLastBrowserControlsState());
    }

    /**
     * Browser controls should not be shown for WebAPKs with 'standalone' display mode while in TWA
     * mode.
     */
    @Test
    public void testStandaloneDisplayMode() {
        mController = buildController(buildWebApkIntentDataProvider(DisplayMode.STANDALONE));
        mController.updateIsInAppMode(true);
        assertEquals(BrowserControlsState.HIDDEN, getLastBrowserControlsState());
    }

    /**
     * Browser controls should be shown for WebAPKs with 'standalone' display mode when outside of
     * WebAPK's scope.
     */
    @Test
    public void testStandaloneDisplayModeOutOfScope() {
        mController = buildController(buildWebApkIntentDataProvider(DisplayMode.STANDALONE));
        mController.updateIsInAppMode(true);
        mController.updateIsInAppMode(false);
        assertEquals(BrowserControlsState.BOTH, getLastBrowserControlsState());
        assertTrue(getLastCloseButtonVisibility());
    }

    /** Browser controls should not be shown for TWAs while in TWA mode. */
    @Test
    public void testTwa() {
        var intent = buildTwaIntent();
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_DISPLAY_MODE,
                new TrustedWebActivityDisplayMode.DefaultMode().toBundle());
        mController = buildController(buildCustomTabIntentProvider(intent));

        mController.updateIsInAppMode(true);
        assertEquals(BrowserControlsState.HIDDEN, getLastBrowserControlsState());
    }

    /** Browser controls should be shown for TWAs when outside of the TWA's scope. */
    @Test
    public void testTwaOutOfScope() {
        mController = buildController(mock(BrowserServicesIntentDataProvider.class));
        mController.updateIsInAppMode(true);
        mController.updateIsInAppMode(false);
        assertEquals(
                "Browser controls should be visible",
                BrowserControlsState.BOTH,
                getLastBrowserControlsState());
        assertTrue("Close button should be visible", getLastCloseButtonVisibility());
    }

    @Test
    public void testTwaMinimalUi_KeepBrowserControlsHidden() {
        var intent = buildTwaIntent();
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_DISPLAY_MODE,
                new TrustedWebActivityDisplayMode.MinimalUiMode().toBundle());
        mController = buildController(buildCustomTabIntentProvider(intent));

        mController.updateIsInAppMode(true);
        assertEquals(
                "Browser controls should be hidden",
                BrowserControlsState.HIDDEN,
                getLastBrowserControlsState());
        assertTrue(
                "Close button should be visible for future layout", getLastCloseButtonVisibility());
    }

    private void setTabSecurityLevel(int securityLevel) {
        doReturn(securityLevel).when(mController).getSecurityLevel(any());
    }

    private BrowserServicesIntentDataProvider buildWebApkIntentDataProvider(
            @DisplayMode.EnumType int displayMode) {
        return new WebApkIntentDataProviderBuilder("org.chromium.webapk.abcd", "https://pwa.rocks/")
                .setDisplayMode(displayMode)
                .build();
    }

    private Intent buildTwaIntent() {
        CustomTabsSession session =
                CustomTabsSession.createMockSessionForTesting(
                        new ComponentName(mContext, ChromeLauncherActivity.class));
        var intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);
        return intent;
    }

    private CustomTabIntentDataProvider buildCustomTabIntentProvider(Intent intent) {
        return new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
    }

    private TrustedWebActivityBrowserControlsVisibilityManager buildController(
            BrowserServicesIntentDataProvider intentDataProvider) {
        return spy(
                new TrustedWebActivityBrowserControlsVisibilityManager(
                        mTabObserverRegistrar,
                        mTabProvider,
                        mToolbarCoordinator,
                        mCloseButtonVisibilityManager,
                        intentDataProvider));
    }

    /** Returns the current browser controls state. */
    private @BrowserControlsState int getLastBrowserControlsState() {
        ArgumentCaptor<Integer> lastBrowserControlsState = ArgumentCaptor.forClass(Integer.class);
        verify(mToolbarCoordinator, atLeast(0))
                .setBrowserControlsState(lastBrowserControlsState.capture());
        return lastBrowserControlsState.getAllValues().isEmpty()
                ? TrustedWebActivityBrowserControlsVisibilityManager.DEFAULT_BROWSER_CONTROLS_STATE
                : lastBrowserControlsState.getValue();
    }

    /** Returns the current close button visibility. */
    private boolean getLastCloseButtonVisibility() {
        ArgumentCaptor<Boolean> lastCloseButtonVisiblity = ArgumentCaptor.forClass(Boolean.class);
        verify(mCloseButtonVisibilityManager, atLeast(1))
                .setVisibility(lastCloseButtonVisiblity.capture());
        return lastCloseButtonVisiblity.getValue();
    }
}
