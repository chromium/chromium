// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;

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
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.edge_to_edge.layout.EdgeToEdgeBaseLayout;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({
    ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE,
    ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE + ":enable_standalone/true/enable_twa/true"
})
@DoNotBatch(reason = "Tests exercise edge-to-edge window state applied at activity startup.")
// TWAs run in a windowed container on tablets/desktop, where the cutout and status bar inset
// expectations below do not hold. Automotive keeps edge-to-edge everywhere disabled, so no
// EdgeToEdgeBaseLayout is inflated.
@DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP)
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
// The coordinated cutout path requires edge-to-edge everywhere, unsupported pre-R.
@MinAndroidSdkLevel(Build.VERSION_CODES.R)
public class TrustedWebActivityEdgeToEdgeTest {
    private final CustomTabActivityTestRule mActivityRule = new CustomTabActivityTestRule();
    private final EmbeddedTestServerRule mServerRule = new EmbeddedTestServerRule();

    @Rule
    public final RuleChain mRules =
            RuleChain.emptyRuleChain().around(mActivityRule).around(mServerRule);

    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized();
        mServerRule.setServerUsesHttps(true);
        Uri mapToUri = Uri.parse(mServerRule.getServer().getURL("/"));
        CommandLine.getInstance()
                .appendSwitchWithValue(
                        ContentSwitches.HOST_RESOLVER_RULES, "MAP * " + mapToUri.getAuthority());
    }

    private CustomTabActivity launchTwa(TrustedWebActivityDisplayMode mode, String page)
            throws TimeoutException {
        String url = mServerRule.getServer().getURL(page);
        Intent intent =
                TrustedWebActivityTestUtil.createTrustedWebActivityIntentAndVerifiedSession(
                        url, ContextUtils.getApplicationContext().getPackageName());
        intent.putExtra(TrustedWebActivityIntentBuilder.EXTRA_DISPLAY_MODE, mode.toBundle());
        mActivityRule.startCustomTabActivityWithIntent(intent);
        CustomTabActivity activity = mActivityRule.getActivity();
        assertTrue(activity.getIntentDataProvider().isTrustedWebActivity());
        return activity;
    }

    private EdgeToEdgeBaseLayout findEdgeToEdgeLayout(View root) {
        if (root instanceof EdgeToEdgeBaseLayout layout) return layout;
        if (root instanceof ViewGroup group) {
            for (int i = 0; i < group.getChildCount(); i++) {
                EdgeToEdgeBaseLayout layout = findEdgeToEdgeLayout(group.getChildAt(i));
                if (layout != null) return layout;
            }
        }
        return null;
    }

    @Test
    @MediumTest
    public void standaloneAutoKeepsFittedRoot() throws TimeoutException {
        CustomTabActivity activity =
                launchTwa(
                        new TrustedWebActivityDisplayMode.DefaultMode(),
                        "/chrome/test/data/android/edge_to_edge/viewport-fit-auto.html");
        EdgeToEdgeBaseLayout layout = findEdgeToEdgeLayout(activity.getWindow().getDecorView());
        assertNotNull(layout);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            assumeNonNull(activity.getWebappInsetsConsumer())
                                    .getDrawEdgeToEdgeSupplier()
                                    .get(),
                            Matchers.is(false));
                    Criteria.checkThat(layout.getPaddingTop(), Matchers.greaterThan(0));
                });
    }

    @Test
    @MediumTest
    public void standaloneCoverDoesNotPadRoot() throws TimeoutException {
        CustomTabActivity activity =
                launchTwa(
                        new TrustedWebActivityDisplayMode.DefaultMode(),
                        "/chrome/test/data/android/edge_to_edge/viewport-fit-cover.html");
        EdgeToEdgeBaseLayout layout = findEdgeToEdgeLayout(activity.getWindow().getDecorView());
        assertNotNull(layout);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            assumeNonNull(activity.getWebappInsetsConsumer())
                                    .getDrawEdgeToEdgeSupplier()
                                    .get(),
                            Matchers.is(true));
                    Criteria.checkThat(layout.getPaddingTop(), Matchers.is(0));
                });
    }

    @Test
    @MediumTest
    public void standaloneCoverReleasesTokenWhenViewportFitResets()
            throws ExecutionException, TimeoutException {
        CustomTabActivity activity =
                launchTwa(
                        new TrustedWebActivityDisplayMode.DefaultMode(),
                        "/chrome/test/data/android/edge_to_edge/viewport-fit-cover.html");
        EdgeToEdgeBaseLayout layout = findEdgeToEdgeLayout(activity.getWindow().getDecorView());
        assertNotNull(layout);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(layout.getPaddingTop(), Matchers.is(0)));

        ChromeTabUtils.loadUrlOnUiThread(
                activity.getActivityTab(),
                mServerRule
                        .getServer()
                        .getURL("/chrome/test/data/android/edge_to_edge/viewport-fit-auto.html"));

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            assumeNonNull(activity.getWebappInsetsConsumer())
                                    .getDrawEdgeToEdgeSupplier()
                                    .get(),
                            Matchers.is(false));
                    Criteria.checkThat(layout.getPaddingTop(), Matchers.greaterThan(0));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void immersiveCoverDoesNotPadRoot() throws TimeoutException {
        CustomTabActivity activity =
                launchTwa(
                        new TrustedWebActivityDisplayMode.ImmersiveMode(
                                false,
                                WindowManager.LayoutParams
                                        .LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES),
                        "/chrome/test/data/android/edge_to_edge/viewport-fit-cover.html");
        EdgeToEdgeBaseLayout layout = findEdgeToEdgeLayout(activity.getWindow().getDecorView());
        assertNotNull(layout);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            assumeNonNull(activity.getWebappInsetsConsumer())
                                    .getDrawEdgeToEdgeSupplier()
                                    .get(),
                            Matchers.is(true));
                    Criteria.checkThat(layout.getPaddingTop(), Matchers.is(0));
                });
    }
}
