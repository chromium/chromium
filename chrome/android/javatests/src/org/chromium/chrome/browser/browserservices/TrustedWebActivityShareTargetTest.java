// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createSession;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createTrustedWebActivityIntent;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.spoofVerification;

import android.content.Intent;
import android.support.test.filters.MediumTest;

import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
import androidx.browser.trusted.sharing.ShareData;
import androidx.browser.trusted.sharing.ShareTarget;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.WebApkPostShareTargetNavigator;
import org.chromium.chrome.browser.webapps.WebApkPostShareTargetNavigatorJni;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.Collections;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TrustedWebActivityShareTargetTest {
    // We are not actually navigating to POST target, so ok not to use test pages here.
    private static final ShareTarget POST_SHARE_TARGET =
            new ShareTarget("https://pwa.rocks/share.html", "POST", null,
                    new ShareTarget.Params("received_title", "received_text", null));

    private static final ShareTarget UNVERIFIED_ORIGIN_POST_SHARE_TARGET =
            new ShareTarget("https://random.website/share.html", "POST", null,
                    new ShareTarget.Params("received_title", "received_text", null));

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String SHARE_TEST_PAGE = "/chrome/test/data/android/about.html";
    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    private final MockPostNavigatorNatives mPostNavigatorNatives = new MockPostNavigatorNatives();

    private final CallbackHelper mPostNavigatorCallback = new CallbackHelper();

    private Intent mIntent;
    private ShareTarget mGetShareTarget;

    // Expected URL when using mGetShareTarget as input.
    private String mExpectedGetRequestUrl;

    @Before
    public void setUp() throws Exception {
        mJniMocker.mock(WebApkPostShareTargetNavigatorJni.TEST_HOOKS, mPostNavigatorNatives);

        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        mEmbeddedTestServerRule.setServerUsesHttps(true);
        String testPage = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
        String shareTestPage = mEmbeddedTestServerRule.getServer().getURL(SHARE_TEST_PAGE);
        mGetShareTarget = new ShareTarget(shareTestPage, "GET", null,
                new ShareTarget.Params("received_title", "received_text", null));
        mExpectedGetRequestUrl = shareTestPage
                + "?received_title=test_title&received_text=test_text";
        spoofVerification(PACKAGE_NAME, testPage);
        spoofVerification(PACKAGE_NAME, "https://pwa.rocks");
        mIntent = createTrustedWebActivityIntent(testPage);
        createSession(mIntent, PACKAGE_NAME);
    }

    @Test
    @MediumTest
    public void sharesDataWithGet_FromInitialIntent() {
        putShareData(mIntent, mGetShareTarget);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(mIntent);
        assertGetRequestUrl(mExpectedGetRequestUrl);
    }

    @Test
    @MediumTest
    public void sharesDataWithPost_FromInitialIntent() throws Exception {
        putShareData(mIntent, POST_SHARE_TARGET);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(mIntent);
        assertPostNavigatorCalled();
    }

    @Test
    @MediumTest
    public void sharesDataWithPost_FromNewIntent() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(mIntent);
        putShareData(mIntent, POST_SHARE_TARGET);
        deliverNewIntent(mIntent);

        assertPostNavigatorCalled();
    }

    @Test
    @MediumTest
    public void sharesDataWithGet_FromNewIntent() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(mIntent);
        putShareData(mIntent, mGetShareTarget);

        deliverNewIntent(mIntent);

        assertGetRequestUrl(mExpectedGetRequestUrl);
    }

    @Test(expected = TimeoutException.class)
    @MediumTest
    public void doesntShareWithUnverifiedOrigin() throws Exception {
        putShareData(mIntent, UNVERIFIED_ORIGIN_POST_SHARE_TARGET);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(mIntent);
        mPostNavigatorCallback.waitForCallback(0, 1, 1000, TimeUnit.MILLISECONDS);
    }

    private void putShareData(Intent intent, ShareTarget shareTarget) {
        ShareData shareData = new ShareData("test_title", "test_text", Collections.emptyList());
        intent.putExtra(TrustedWebActivityIntentBuilder.EXTRA_SHARE_DATA, shareData.toBundle());
        intent.putExtra(TrustedWebActivityIntentBuilder.EXTRA_SHARE_TARGET, shareTarget.toBundle());
    }

    private void assertGetRequestUrl(final String expectedGetRequestUrl) {
        // startCustomTabActivityWithIntent waits for native, so the tab must be present already.
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(tab, expectedGetRequestUrl);
    }

    private void assertPostNavigatorCalled() throws TimeoutException {
        // Constructing POST requests is unit-tested elsewhere.
        // Here we only care that the request reaches the native code.
        mPostNavigatorCallback.waitForCallback(0);
    }

    private void deliverNewIntent(Intent intent) {
        // Delivering intents to existing CustomTabActivity in tests is error-prone and out of scope
        // of these tests. Thus calling onNewIntent directly.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mCustomTabActivityTestRule.getActivity().onNewIntent(intent));
    }

    private class MockPostNavigatorNatives implements WebApkPostShareTargetNavigator.Natives {
        @Override
        public void nativeLoadViewForShareTargetPost(boolean isMultipartEncoding, String[] names,
                String[] values, boolean[] isValueFileUris, String[] filenames, String[] types,
                String startUrl, WebContents webContents) {
            mPostNavigatorCallback.notifyCalled();
        }
    }
}
