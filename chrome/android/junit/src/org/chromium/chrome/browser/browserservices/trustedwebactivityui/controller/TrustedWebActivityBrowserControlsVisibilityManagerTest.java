// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

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

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CloseButtonVisibilityManager;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;

/** Tests for {@link TrustedWebActivityBrowserControlsVisibilityManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrustedWebActivityBrowserControlsVisibilityManagerTest {
    @Mock public TabObserverRegistrar mTabObserverRegistrar;
    @Mock public CustomTabActivityTabProvider mTabProvider;
    @Mock public Tab mTab;
    @Mock SecurityStateModel.Natives mSecurityStateMocks;
    @Mock public CustomTabToolbarCoordinator mToolbarCoordinator;
    @Mock public CloseButtonVisibilityManager mCloseButtonVisibilityManager;

    @Mock TrustedWebActivityBrowserControlsVisibilityManager mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        SecurityStateModelJni.TEST_HOOKS.setInstanceForTesting(mSecurityStateMocks);
        when(mTabProvider.getTab()).thenReturn(mTab);
        doReturn(Tab.INVALID_TAB_ID).when(mTab).getParentId();
        setTabSecurityLevel(ConnectionSecurityLevel.NONE);
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

    /** Browser controls should be shown for WebAPKs with 'minimal-ui' display mode. */
    @Test
    public void testMinimalUiDisplayMode() {
        mController = buildController(buildWebApkIntentDataProvider(DisplayMode.MINIMAL_UI));
        mController.updateIsInAppMode(true);
        assertEquals(BrowserControlsState.BOTH, getLastBrowserControlsState());
        assertFalse(getLastCloseButtonVisibility());
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
        mController = buildController(mock(BrowserServicesIntentDataProvider.class));
        mController.updateIsInAppMode(true);
        assertEquals(BrowserControlsState.HIDDEN, getLastBrowserControlsState());
    }

    /** Browser controls should be shown for TWAs when outside of the TWA's scope. */
    @Test
    public void testTwaOutOfScope() {
        mController = buildController(mock(BrowserServicesIntentDataProvider.class));
        mController.updateIsInAppMode(true);
        mController.updateIsInAppMode(false);
        assertEquals(BrowserControlsState.BOTH, getLastBrowserControlsState());
        assertTrue(getLastCloseButtonVisibility());
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
