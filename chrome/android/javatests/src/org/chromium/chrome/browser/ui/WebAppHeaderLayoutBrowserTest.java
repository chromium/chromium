// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;

import androidx.browser.trusted.TrustedWebActivityDisplayMode;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil;
import org.chromium.chrome.browser.customtabs.BaseCustomTabRootUiCoordinator;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderCoordinator;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.ui.web_app_header.WebAppHeaderLayoutCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Locale;
import java.util.concurrent.TimeoutException;

@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "https://crbug.com/1454648")
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@MinAndroidSdkLevel(Build.VERSION_CODES.VANILLA_ICE_CREAM)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.DisableFeatures({ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE})
@Features.EnableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
public class WebAppHeaderLayoutBrowserTest {

    @Rule public CustomTabActivityTestRule mActivityTestRule = new CustomTabActivityTestRule();

    @Rule public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public OverrideContextWrapperTestRule mAutomotiveRule = new OverrideContextWrapperTestRule();

    @Rule
    public RuleChain mRuleChain =
            RuleChain.emptyRuleChain()
                    .around(mActivityTestRule)
                    .around(mEmbeddedTestServerRule)
                    .around(mAutomotiveRule);

    private static final String ROOT_TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_OUT_OF_SCOPE =
            "https://out-of-scope.com/chrome/test/data/android/theme_color_test.html";
    private static final String TEST_PAGE_WITH_CERT_ERROR =
            "https://certificateerror.com/chrome/test/data/android/theme_color_test.html";

    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    private static final int APP_HEADER_LEFT_PADDING = 10;
    private static final int APP_HEADER_RIGHT_PADDING = 20;

    private String mTestPage;
    private final Rect mWidestUnoccludedRect = new Rect();
    private final Rect mWindowRect = new Rect();

    @Before
    public void setUp() throws TimeoutException {
        LibraryLoader.getInstance().ensureInitialized();

        mEmbeddedTestServerRule.setServerUsesHttps(true); // TWAs only work with HTTPS.
        mTestPage = mEmbeddedTestServerRule.getServer().getURL(ROOT_TEST_PAGE);

        // Map non-localhost-URLs to localhost. Navigations to non-localhost URLs will throw a
        // certificate error.
        Uri mapToUri = Uri.parse(mEmbeddedTestServerRule.getServer().getURL("/"));
        CommandLine.getInstance()
                .appendSwitchWithValue(
                        ContentSwitches.HOST_RESOLVER_RULES, "MAP * " + mapToUri.getAuthority());
    }

    @Test
    @MediumTest
    public void testMinUiTwaInDesktopWindowing_ShowWebAppHeader_HideToolbar()
            throws TimeoutException {
        Intent intent =
                TrustedWebActivityTestUtil.createTrustedWebActivityIntentAndVerifiedSession(
                        mTestPage, PACKAGE_NAME);
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_DISPLAY_MODE,
                new TrustedWebActivityDisplayMode.MinimalUiMode().toBundle());
        mActivityTestRule.startCustomTabActivityWithIntent(intent);

        // Start in fullscreen and verify that header is not visible.
        triggerDesktopWindowingModeChange(/* isInDesktopWindow= */ false);
        verifyHeaderVisibility(false);
        verifyBrowserControlsVisibility(false);

        // Switch to desktop windowing mode and verify that header is visible.
        triggerDesktopWindowingModeChange(/* isInDesktopWindow= */ true);
        verifyHeaderVisibility(true);
        verifyBrowserControlsVisibility(false);
    }

    @Test
    @MediumTest
    public void testMinUiInDesktopWindowingFailedVerification_ShowWebHeader_ShowToolbar()
            throws TimeoutException {
        Intent intent =
                TrustedWebActivityTestUtil.createTrustedWebActivityIntentAndVerifiedSession(
                        mTestPage, PACKAGE_NAME);
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_DISPLAY_MODE,
                new TrustedWebActivityDisplayMode.MinimalUiMode().toBundle());
        mActivityTestRule.startCustomTabActivityWithIntent(intent);

        // Start in desktop windowing and verify controls and header visibility.
        triggerDesktopWindowingModeChange(/* isInDesktopWindow= */ true);
        verifyHeaderVisibility(true);
        verifyBrowserControlsVisibility(false);

        // Navigate out of scope.
        mActivityTestRule.loadUrl(TEST_PAGE_WITH_CERT_ERROR);

        // Verify out-of-scope banner is shown.
        verifyHeaderVisibility(true);
        verifyBrowserControlsVisibility(true);
    }

    @Test
    @MediumTest
    public void testMinUI_BackPress_NavigateToPreviousPage() throws TimeoutException {
        // Start TWA in desktop windowing mode.
        Intent intent =
                TrustedWebActivityTestUtil.createTrustedWebActivityIntentAndVerifiedSession(
                        mTestPage, PACKAGE_NAME);
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_DISPLAY_MODE,
                new TrustedWebActivityDisplayMode.MinimalUiMode().toBundle());
        mActivityTestRule.startCustomTabActivityWithIntent(intent);
        triggerDesktopWindowingModeChange(/* isInDesktopWindow= */ true);

        // Verify that browser controls are shown for the page that's out of scope of the web app.
        var tab = mActivityTestRule.getActivity().getActivityTab();
        mActivityTestRule.loadUrl(TEST_PAGE_OUT_OF_SCOPE);
        verifyBrowserControlsVisibility(true);

        // Click back and verify that previous page is shown that is in scope of the web app.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var webAppHeaderCoordinator = getWebAppHeaderLayoutCoordinator();
                    webAppHeaderCoordinator
                            .getWebAppHeaderLayout()
                            .findViewById(R.id.back_button)
                            .performClick();
                });

        ChromeTabUtils.waitForTabPageLoaded(tab, mTestPage);
        verifyBrowserControlsVisibility(false);
    }

    private @BrowserControlsState int getBrowserControlConstraints(Tab tab) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> TabBrowserControlsConstraintsHelper.getConstraints(tab));
    }

    private void verifyHeaderVisibility(boolean isVisible) {
        var webAppHeaderCoordinator = getWebAppHeaderLayoutCoordinator();
        assertNotNull("Web app header coordinator should be initialized", webAppHeaderCoordinator);
        CriteriaHelper.pollUiThread(
                () -> {
                    var message =
                            String.format(
                                    Locale.US,
                                    "Web app header should be %s.",
                                    isVisible ? "shown" : "hidden");
                    if (isVisible) assertTrue(message, webAppHeaderCoordinator.isVisible());
                    else assertFalse(message, webAppHeaderCoordinator.isVisible());
                },
                10000,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private WebAppHeaderLayoutCoordinator getWebAppHeaderLayoutCoordinator() {
        var rootUiCoordinator =
                (BaseCustomTabRootUiCoordinator)
                        mActivityTestRule.getActivity().getRootUiCoordinatorForTesting();
        return rootUiCoordinator.getWebAppHeaderLayoutCoordinator();
    }

    private void verifyBrowserControlsVisibility(boolean isVisible) {
        var tab = mActivityTestRule.getActivity().getActivityTab();
        int controlsState = isVisible ? BrowserControlsState.SHOWN : BrowserControlsState.HIDDEN;
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                getBrowserControlConstraints(tab), Matchers.is(controlsState)),
                10000,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void triggerDesktopWindowingModeChange(boolean isInDesktopWindow) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var appHeaderStateProvider =
                            mActivityTestRule
                                    .getActivity()
                                    .getRootUiCoordinatorForTesting()
                                    .getDesktopWindowStateManager();
                    setupAppHeaderRects(isInDesktopWindow);
                    var appHeaderState =
                            new AppHeaderState(
                                    mWindowRect, mWidestUnoccludedRect, isInDesktopWindow);
                    ((AppHeaderCoordinator) appHeaderStateProvider)
                            .setStateForTesting(
                                    isInDesktopWindow, appHeaderState, /* isFocused= */ true);
                    AppHeaderUtils.setAppInDesktopWindowForTesting(isInDesktopWindow);
                });
    }

    private void setupAppHeaderRects(boolean isInDesktopWindow) {
        var activity = mActivityTestRule.getActivity();
        activity.getWindow().getDecorView().getGlobalVisibleRect(mWindowRect);
        if (isInDesktopWindow) {
            var height =
                    mActivityTestRule
                            .getActivity()
                            .getResources()
                            .getDimensionPixelSize(R.dimen.web_app_header_min_height);
            mWidestUnoccludedRect.set(
                    APP_HEADER_LEFT_PADDING,
                    0,
                    mWindowRect.right - APP_HEADER_RIGHT_PADDING,
                    height);
        } else {
            mWidestUnoccludedRect.setEmpty();
        }
    }
}
