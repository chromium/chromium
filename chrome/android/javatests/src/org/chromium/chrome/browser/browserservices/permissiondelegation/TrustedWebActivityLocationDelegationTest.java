// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.isTrustedWebActivity;

import android.net.Uri;
import android.os.RemoteException;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule.PermissionUpdateWaiter;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.test.MockCertVerifierRuleAndroid;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Tests TrustedWebActivity location delegation. */
@RunWith(BaseJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tasks run at TWA startup.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TrustedWebActivityLocationDelegationTest {
    public final CustomTabActivityTestRule mCustomTabActivityTestRule =
            new CustomTabActivityTestRule();

    public final MockCertVerifierRuleAndroid mCertVerifierRule =
            new MockCertVerifierRuleAndroid(0 /* net::OK */);

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.emptyRuleChain().around(mCustomTabActivityTestRule).around(mCertVerifierRule);

    // Origin of the test_trusted_web_activity.
    private static final String TEST_ORIGIN = "www.example.com";
    private static final String TEST_FILE = "/content/test/data/android/geolocation.html";
    private static final String TEST_SUPPORT_PACKAGE = "org.chromium.chrome.tests.support";

    private String mTestPage;

    @Before
    public void setUp() throws TimeoutException, RemoteException {
        mCustomTabActivityTestRule.setFinishActivity(true);
        // Initialize native.
        LibraryLoader.getInstance().ensureInitialized();

        mCustomTabActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        Uri mapToUri = Uri.parse(mCustomTabActivityTestRule.getTestServer().getURL("/"));
        CommandLine.getInstance()
                .appendSwitchWithValue(
                        ContentSwitches.HOST_RESOLVER_RULES, "MAP * " + mapToUri.getAuthority());

        mTestPage =
                mCustomTabActivityTestRule
                        .getTestServer()
                        .getURLWithHostName(TEST_ORIGIN, TEST_FILE);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                TrustedWebActivityTestUtil.createTrustedWebActivityIntentAndVerifiedSession(
                        mTestPage, TEST_SUPPORT_PACKAGE));
    }

    @Test
    @MediumTest
    public void getLocationFromTestTwaService() throws TimeoutException, Exception {
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        PermissionUpdateWaiter updateWaiter =
                new PermissionUpdateWaiter("Count:", mCustomTabActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(updateWaiter);
                });
        getGeolocation();
        updateWaiter.waitForNumUpdates(1);
    }

    @Test
    @MediumTest
    public void getLocationFromChrome_noTwaService() throws TimeoutException, Exception {
        String packageName = "other.package.name";
        String testPage =
                mCustomTabActivityTestRule
                        .getTestServer()
                        .getURLWithHostName("www.otherexample.com", TEST_FILE);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                TrustedWebActivityTestUtil.createTrustedWebActivityIntentAndVerifiedSession(
                        testPage, packageName));

        assertTrue(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));

        verifyLocationFromChrome();
    }

    @Test
    @MediumTest
    public void getLocationFromChrome_afterNavigateAwayFromTrustedOrigin()
            throws TimeoutException, Exception {
        String other_page =
                mCustomTabActivityTestRule
                        .getTestServer()
                        .getURLWithHostName("www.otherexample.com", TEST_FILE);

        mCustomTabActivityTestRule.loadUrl(other_page);
        assertFalse(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));

        verifyLocationFromChrome();
    }

    private void getGeolocation() throws TimeoutException {
        mCustomTabActivityTestRule.runJavaScriptCodeInCurrentTab("initiate_getCurrentPosition()");
    }

    private void setAllowChromeSiteLocation(GURL url, boolean allowed) {
        @ContentSettingValues
        int setting = allowed ? ContentSettingValues.ALLOW : ContentSettingValues.BLOCK;
        Profile profile = mCustomTabActivityTestRule.getProfile(false);
        PermissionInfo info =
                new PermissionInfo(
                        ContentSettingsType.GEOLOCATION,
                        url.getSpec(),
                        /* embedder= */ null,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE);

        ThreadUtils.runOnUiThreadBlocking(() -> info.setContentSetting(profile, setting));

        CriteriaHelper.pollUiThread(
                () -> {
                    return info.getContentSetting(profile) == setting;
                });
    }

    private void verifyLocationFromChrome() throws Exception {
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();

        setAllowChromeSiteLocation(tab.getUrl(), false);
        PermissionUpdateWaiter errorWaiter =
                new PermissionUpdateWaiter("deny", mCustomTabActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(errorWaiter);
                });
        getGeolocation();
        errorWaiter.waitForNumUpdates(0);

        setAllowChromeSiteLocation(tab.getUrl(), true);
        PermissionUpdateWaiter updateWaiter =
                new PermissionUpdateWaiter("Count:", mCustomTabActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(updateWaiter);
                });
        getGeolocation();
        updateWaiter.waitForNumUpdates(1);
    }
}
