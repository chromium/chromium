// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.net.Uri;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

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
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.io.IOException;

/** Render tests for the Custom Tab and Trusted Web Activity App Menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "App menu and TWA initialization needs isolated activity lifecycle")
public class CustomTabAppMenuRenderTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";

    public final CustomTabActivityTestRule mCustomTabActivityTestRule =
            new CustomTabActivityTestRule();
    public final EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_APP_MENU)
                    .build();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.emptyRuleChain()
                    .around(mCustomTabActivityTestRule)
                    .around(mEmbeddedTestServerRule)
                    .around(mRenderTestRule);

    private String mTestPage;

    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized();
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

        mEmbeddedTestServerRule.setServerUsesHttps(true);
        mTestPage = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);

        Uri mapToUri = Uri.parse(mEmbeddedTestServerRule.getServer().getURL("/"));
        CommandLine.getInstance()
                .appendSwitchWithValue(
                        ContentSwitches.HOST_RESOLVER_RULES, "MAP * " + mapToUri.getAuthority());
    }

    private Intent createTwaIntent() throws Exception {
        Intent intent = TrustedWebActivityTestUtil.createTrustedWebActivityIntent(mTestPage);
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        TrustedWebActivityTestUtil.spoofVerification(packageName, mTestPage);
        TrustedWebActivityTestUtil.createSession(intent, packageName);
        return intent;
    }

    private void launchAndRenderMenu(Intent intent, String renderTestId) throws IOException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(mCustomTabActivityTestRule.getActivity());
        View menuView =
                AppMenuTestSupport.getListView(mCustomTabActivityTestRule.getAppMenuCoordinator());
        mRenderTestRule.render(menuView, renderTestId);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({ChromeFeatureList.ANDROID_PAGE_INFO_AS_APP_MENU_ITEM})
    public void testTwaAppMenu() throws Exception {
        Intent intent = createTwaIntent();
        launchAndRenderMenu(intent, "twa_app_menu");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({ChromeFeatureList.ANDROID_PAGE_INFO_AS_APP_MENU_ITEM})
    public void testCctAppMenu() throws Exception {
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), mTestPage);
        launchAndRenderMenu(intent, "cct_app_menu");
    }
}
