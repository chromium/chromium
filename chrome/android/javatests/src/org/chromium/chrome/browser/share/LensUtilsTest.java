// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

/**
 * Tests of {@link LensUtils}.
 * TODO(https://crbug.com/1054738): Reimplement LensUtilsTest as robolectric tests
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class LensUtilsTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private static final String TEST_MIN_AGSA_VERSION = "11.34";
    private static final String TEST_MIN_AGSA_VERSION_BELOW_DIRECT_INTENT_MIN = "11.33.9";

    @Mock
    Context mContext;
    @Mock
    PackageManager mPackageManager;
    @Mock
    PackageInfo mPackageInfo;

    @Before
    public void setUp() throws NameNotFoundException {
        MockitoAnnotations.initMocks(this);
        doReturn(mContext).when(mContext).getApplicationContext();
        doReturn(mPackageManager).when(mContext).getPackageManager();
        doReturn(mPackageInfo).when(mPackageManager).getPackageInfo(IntentHandler.PACKAGE_GSA, 0);
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when user is signed
     * in.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentSignedInTest() {
        mBrowserTestRule.addTestAccountThenSigninAndEnableSync();

        Intent intentNoUri = getShareWithGoogleLensIntentOnUiThread(Uri.EMPTY,
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* pageUrl */ null, LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM,
                /* requiresConfirmation */ false);
        Assert.assertEquals("Intent without image has incorrect URI", "googleapp://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* pageUrl */ null, LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM,
                /* requiresConfirmation */ false);
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "test%40gmail.com&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#isGoogleLensFeatureEnabled()} method when disable incognito param is
     * unset and user is incognito.
     */
    @Test
    @SmallTest
    public void isGoogleLensFeatureEnabled_incognitoParamUnsetIncognitoUser() {
        Assert.assertFalse("Feature incorrectly enabled when incognito param is not set",
                isGoogleLensFeatureEnabledOnUiThread(true));
    }

    /**
     * Test {@link LensUtils#isGoogleLensFeatureEnabled()} method when incognito users are enabled
     * and user is incognito.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/false"})
    @Test
    @SmallTest
    public void
    isGoogleLensFeatureEnabled_incognitoEnabledIncognitoUser() {
        Assert.assertTrue("Feature incorrectly disabled when incognito param is not set",
                isGoogleLensFeatureEnabledOnUiThread(true));
    }

    /**
     * Test {@link LensUtils#isGoogleLensFeatureEnabled()} method when incognito users are disabled
     * and user is incognito.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/true"})
    @Test
    @SmallTest
    public void
    isGoogleLensFeatureEnabled_incognitoDisabledIncognitoUser() {
        Assert.assertFalse("Feature incorrectly not disabled when incognito param was set",
                isGoogleLensFeatureEnabledOnUiThread(true));
    }

    /**
     * Test {@link LensUtils#isGoogleLensFeatureEnabled()} method when incognito users are disabled
     * and user is not incognito.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/true"})
    @Test
    @SmallTest
    public void
    isGoogleLensFeatureEnabled_incognitoDisabledStandardUser() {
        Assert.assertTrue("Feature incorrectly disabled when user was not incognito",
                isGoogleLensFeatureEnabledOnUiThread(false));
    }

    /**
     * Test {@link LensUtils#isGoogleLensFeatureEnabled()} method when disable incognito param is
     * unset and user is incognito.
     */
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:lensShopVariation/ShopSimilarProducts"})
    @Test
    @SmallTest
    public void
    isGoogleLensShoppingFeatureEnabled_incognitoParamUnsetIncognitoUser() {
        Assert.assertFalse("Feature incorrectly enabled when incognito param is not set",
                isGoogleLensShoppingFeatureEnabledOnUiThread(true));
    }

    /**
     * Test {@link LensUtils#isGoogleLensShoppingFeatureEnabled()} method when incognito users are
     * enabled and user is incognito.
     */
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/false/"
                    + "lensShopVariation/ShopSimilarProducts"})
    @Test
    @SmallTest
    public void
    isGoogleLensShoppingFeatureEnabled_incognitoEnabledIncognitoUser() {
        Assert.assertTrue("Feature incorrectly disabled when incognito param is not set",
                isGoogleLensShoppingFeatureEnabledOnUiThread(true));
    }

    /**
     * Test {@link LensUtils#isGoogleLensFeatureEnabled()} method when incognito users are disabled
     * and user is incognito.
     */
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/true/"
                    + "lensShopVariation/ShopSimilarProducts"})
    @Test
    @SmallTest
    public void
    isGoogleLensShoppingFeatureEnabled_incognitoDisabledIncognitoUser() {
        Assert.assertFalse("Feature incorrectly not disabled when incognito param was set",
                isGoogleLensShoppingFeatureEnabledOnUiThread(true));
    }

    /**
     * Test {@link LensUtils#isGoogleLensFeatureEnabled()} method when incognito users are disabled
     * and user is not incognito.
     */
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/true/"
                    + "lensShopVariation/ShopSimilarProducts"})
    @Test
    @SmallTest
    public void
    isGoogleLensShoppingFeatureEnabled_incognitoDisabledStandardUser() {
        Assert.assertTrue("Feature incorrectly disabled when user was not incognito",
                isGoogleLensShoppingFeatureEnabledOnUiThread(false));
    }

    /**
     * Test {@link LensUtils#isGoogleLensFeatureEnabled()} method when shopping chip is enabled.
     */
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS
                    + "<FakeStudyName," + ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/true/"
                    + "lensShopVariation/ShopSimilarProducts"})
    @Test
    @SmallTest
    public void
    isGoogleLensShoppingFeatureEnabled_shoppingChipEnabled() {
        Assert.assertFalse("Feature incorrectly enabled when shopping chip was enabled",
                isGoogleLensShoppingFeatureEnabledOnUiThread(false));
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when user is signed
     * in and the direct intent experiment is enabled.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:useDirectIntent/true"})
    @Test
    @SmallTest
    public void
    getShareWithGoogleLensIntentSignedInTest_directIntentEnabled() {
        mBrowserTestRule.addTestAccountThenSigninAndEnableSync();
        LensUtils.setFakeInstalledAgsaVersion(TEST_MIN_AGSA_VERSION);
        Intent intentNoUri = getShareWithGoogleLensIntentOnUiThread(Uri.EMPTY,
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* pageUrl */ null, LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM,
                /* requiresConfirmation */ false);
        Assert.assertEquals("Intent without image has incorrect URI", "google://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* pageUrl */ null, LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM,
                /* requiresConfirmation */ false);
        Assert.assertEquals("Intent with image has incorrect URI",
                "google://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url"
                        + "&AccountNameUriKey=test%40gmail.com&IncognitoUriKey=false"
                        + "&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when user is signed
     * in and the direct intent experiment is enabled, but the AGSA version is empty.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:useDirectIntent/true"})
    @Test
    @SmallTest
    public void
    getShareWithGoogleLensIntentSignedInTest_directIntentEnabledAgsaVersionEmpty() {
        mBrowserTestRule.addTestAccountThenSigninAndEnableSync();
        LensUtils.setFakeInstalledAgsaVersion("");
        Intent intentNoUri = getShareWithGoogleLensIntentOnUiThread(Uri.EMPTY,
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* pageUrl */ null, LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM,
                /* requiresConfirmation */ false);
        Assert.assertEquals("Intent without image has incorrect URI", "googleapp://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* pageUrl */ null, LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM,
                /* requiresConfirmation */ false);
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url"
                        + "&AccountNameUriKey=test%40gmail.com&IncognitoUriKey=false"
                        + "&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when user is signed
     * in and the direct intent experiment is enabled, but the AGSA version is below the minimum
     * required for direct intent.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:useDirectIntent/true"})
    @Test
    @SmallTest
    public void
    getShareWithGoogleLensIntentSignedInTest_directIntentEnabledAgsaVersionBelowMinimum() {
        mBrowserTestRule.addTestAccountThenSigninAndEnableSync();
        LensUtils.setFakeInstalledAgsaVersion(TEST_MIN_AGSA_VERSION_BELOW_DIRECT_INTENT_MIN);
        Intent intentNoUri = getShareWithGoogleLensIntentOnUiThread(Uri.EMPTY,
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* pageUrl */ null, LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM,
                /* requiresConfirmation */ false);
        Assert.assertEquals("Intent without image has incorrect URI", "googleapp://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* pageUrl */ null, LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM,
                /* requiresConfirmation */ false);
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url"
                        + "&AccountNameUriKey=test%40gmail.com&IncognitoUriKey=false"
                        + "&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when user is
     * incognito.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentIncognitoTest() {
        mBrowserTestRule.addTestAccountThenSigninAndEnableSync();
        Intent intentNoUri = getShareWithGoogleLensIntentOnUiThread(Uri.EMPTY,
                /* isIncognito= */ true, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* pageUrl */ null, LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM,
                /* requiresConfirmation */ false);
        Assert.assertEquals("Intent without image has incorrect URI", "googleapp://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ true, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* pageUrl */ null, LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM,
                /* requiresConfirmation */ false);
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=true&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method and that variations
     * are added to the URI.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentWithVariationsTest() {
        LensUtils.setFakeVariationsForTesting(" 123 456 ");
        mBrowserTestRule.addTestAccountThenSigninAndEnableSync();

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "", /* pageUrl */ null,
                LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, /* requiresConfirmation */ false);
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "test%40gmail.com&IncognitoUriKey=false&ActivityLaunchTimestampNanos="
                        + "1234&Gid=123%20456",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method and that variations
     * are not sent when the user is incognito.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentWithVariationsIncognitoTest() {
        LensUtils.setFakeVariationsForTesting(" 123 456 ");
        mBrowserTestRule.addTestAccountThenSigninAndEnableSync();

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ true, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "", /* pageUrl */ null,
                LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, /* requiresConfirmation */ false);
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=true&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when user is not
     * signed in.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentNotSignedInTest() {
        Intent intentNoUri = getShareWithGoogleLensIntentOnUiThread(Uri.EMPTY,
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* pageUrl */ null, LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM,
                /* requiresConfirmation */ false);
        Assert.assertEquals("Intent without image has incorrect URI", "googleapp://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* pageUrl */ null, LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM,
                /* requiresConfirmation */ false);
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when the timestamp was
     * unexpectedly 0.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentZeroTimestampTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUriZeroTimestamp =
                getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                        /* isIncognito= */ false, 0L, /* srcUrl */ "", /* titleOrAltText */ "",
                        /* pageUrl */ null, LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM,
                        /* requiresConfirmation */ false);
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=0",
                intentWithContentUriZeroTimestamp.getData().toString());
    }

    private Intent getShareWithGoogleLensIntentOnUiThread(Uri imageUri, boolean isIncognito,
            long currentTimeNanos, String srcUrl, String titleOrAltText, String pageUrl,
            @LensEntryPoint int lensEntryPoint, boolean requiresConfirmation) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> LensUtils.getShareWithGoogleLensIntent(mContext, imageUri, isIncognito,
                                currentTimeNanos, new GURL(srcUrl), titleOrAltText,
                                new GURL(pageUrl), lensEntryPoint, requiresConfirmation));
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when alt text is available but
     * not enabled by finch.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentAltDisabledNotAddedTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "An image description.", /* pageUrl */ null,
                LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, /* requiresConfirmation */ false);
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when alt text is available and
     * enabled by finch, but incognito.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:sendAlt/true"})
    @Test
    @SmallTest
    public void
    getShareWithGoogleLensIntentAltEnabledIncognitoNotAddedTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ true, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "An image description.", /* pageUrl */ null,
                LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, /* requiresConfirmation */ false);
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=true&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when the intent type is
     * shopping.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentWithShoppingIntentTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "", /* pageUrl */ null,
                LensEntryPoint.CONTEXT_MENU_SHOP_MENU_ITEM, /* requiresConfirmation */ false);
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234&"
                        + "lens_intent_type=18",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when requires confirmation.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentWithConfirmationTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "", /* pageUrl */ null,
                LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, /* requiresConfirmation */ true);
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234&"
                        + "requiresConfirmation=true",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when alt text is available and
     * enabled by finch.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:sendAlt/true"})
    @Test
    @SmallTest
    public void
    getShareWithGoogleLensIntentAltEnabledAddedTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "",
                /* titleOrAltText= */ "An image description.", /* pageUrl= */ null,
                LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, /* requiresConfirmation */ false);
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234&ImageAlt="
                        + "An%20image%20description.",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when page url is available and
     * enabled by finch.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:sendPage/true"})
    @Test
    @SmallTest
    public void
    getShareWithGoogleLensIntentPageUrlEnabledAddedTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "",
                /* pageUrl */ "https://www.google.com?testQueryParam",
                LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, /* requiresConfirmation */ false);
        Assert.assertEquals("Intent with image has incorrect Page URL",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234&PageUrl="
                        + "https%3A%2F%2Fwww.google.com%2F%3FtestQueryParam",
                intentWithContentUri.getData().toString());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when page url is available and
     * enabled by finch but the user is incognito.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:sendPage/true"})
    @Test
    @SmallTest
    public void
    getShareWithGoogleLensIntentPageUrlDisabledIncognitoTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ true, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "",
                /* pageUrl */ "https://www.google.com?testQueryParam",
                LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, /* requiresConfirmation */ false);
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=true&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when page url is available and
     * but not added by finch.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentPageUrlDisabledAddedTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "",
                /* pageUrl */ "https://www.google.com?testQueryParam",
                LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, /* requiresConfirmation */ false);
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when image src is available but
     * not enabled by finch.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentSrcDisabledNotAddedTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "http://www.google.com?key=val",
                /* titleOrAltText */ "", /* pageUrl */ null,
                LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, /* requiresConfirmation */ false);
        // The account name should not be included in the intent because the param is disabled..
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when image src is available and
     * enabled by finch.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:sendSrc/true"})
    @Test
    @SmallTest
    public void
    getShareWithGoogleLensIntentSrcEnabledAndAddedTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "http://www.google.com?key=val",
                /* titleOrAltText */ "", /* pageUrl */ null,
                LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, /* requiresConfirmation */ false);
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234&ImageSrc="
                        + "http%3A%2F%2Fwww.google.com%2F%3Fkey%3Dval",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when image src is available,
     * enabled by finch, but user is incognito.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:sendSrc/true"})
    @Test
    @SmallTest
    public void
    getShareWithGoogleLensIntentSrcEnabledIncognitoNotAddedTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ true, 1234L, /* srcUrl */ "http://www.google.com?key=val",
                /* titleOrAltText */ "", /* pageUrl */ null,
                LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, /* requiresConfirmation */ false);
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=true&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#isInShoppingAllowlist(url)} method for url in domain allowlist.
     */
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:allowlistEntries/shopping-site-2"})
    @Test
    @SmallTest
    public void
    isInShoppingAllowlistWithDomainAllowlistTest() {
        final GURL pageUrl = new GURL("https://shopping-site-2.com/product_1");
        assertTrue(isInShoppingAllowlistOnUiThread(pageUrl));
    }

    /**
     * Test {@link LensUtils#isInShoppingAllowlist(url)} method for url with shopping url patterns.
     */
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:shoppingUrlPatterns/"
                    + "^https...shopping-site.*"})
    @Test
    @SmallTest
    public void
    isInShoppingAllowlistWithShoppingUrlPatternsTest() {
        final GURL pageUrl = new GURL("https://shopping-site-2.com/product_1");
        assertTrue(isInShoppingAllowlistOnUiThread(pageUrl));
    }

    /**
     * Test {@link LensUtils#shouldLogUkm(isIncognito)} method for enable UKM logging for Lens chip
     * integration.
     */
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/true/"
                    + "logUkm/true"})
    @Test
    @SmallTest
    public void
    shouldLogUkm_shoppingChipUkmLoggingEnabled() {
        assertTrue(shouldLogUkmOnUiThread(/* isIncognito=*/false));
    }

    /**
     * Test {@link LensUtils#shouldLogUkm(isIncognito)} method for enable UKM logging for Lens chip
     * integration.
     */
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/true/"
                    + "logUkm/false"})
    @Test
    @SmallTest
    public void
    shouldLogUkm_shoppingChipUkmLoggingDisabled() {
        assertFalse(shouldLogUkmOnUiThread(/* isIncognito=*/false));
    }

    /**
     * Test {@link LensUtils#isInShoppingAllowlist(url)} method for url with default shopping url
     * patterns.
     */
    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST})
    public void isInShoppingAllowlistWithDefaultShoppingUrlPatternTest() {
        final GURL googleShoppingItemUrl = new GURL("https://www.google.com/shopping/product_1");
        final GURL googleShoppingPageUrl =
                new GURL("https://www.google.com/search?=8893t5/tbm=shop/dress");
        assertTrue(isInShoppingAllowlistOnUiThread(googleShoppingPageUrl));
        assertTrue(isInShoppingAllowlistOnUiThread(googleShoppingItemUrl));
    }

    /**
     * Test {@link LensUtils#buildLensIntentParams()} method for building intent parameters for
     * startng Lens.
     */
    @Test
    @SmallTest
    public void buildLensIntentParamsTest() {
        final String contentUrl = "content://image-url";
        final boolean isIncognito = false;
        final String srcUrl = "https://www.google.com";
        final String titleOrAltText = "Image Title";
        final String pageUrl = "https://www.google.com";
        final int intentType = 0;
        LensIntentParams lensIntentParams = LensUtils.buildLensIntentParams(Uri.parse(contentUrl),
                isIncognito, srcUrl, titleOrAltText, pageUrl,
                LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, /* requiresConfirmation */ false);

        Assert.assertEquals("Lens intent parameters has incorrect image URI.", contentUrl,
                lensIntentParams.getImageUri().toString());
        Assert.assertEquals("Lens intent parameters has incorrect incognito value.", isIncognito,
                lensIntentParams.getIsIncognito());
        Assert.assertEquals("Lens intent parameters has incorrect src URL.", srcUrl,
                lensIntentParams.getSrcUrl());
        Assert.assertEquals("Lens intent parameters has incorrect title or alt text.",
                titleOrAltText, lensIntentParams.getImageTitleOrAltText());
        Assert.assertEquals("Lens intent parameters has incorrect page URL.", pageUrl,
                lensIntentParams.getPageUrl());
        Assert.assertEquals("Lens intent parameters has incorrect requires confirmation value.",
                false, lensIntentParams.getRequiresConfirmation());
        Assert.assertEquals("Lens intent parameters has incorrect intent type value.", intentType,
                lensIntentParams.getIntentType());
    }

    private boolean isInShoppingAllowlistOnUiThread(GURL imageUri) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> LensUtils.isInShoppingAllowlist(imageUri));
    }

    private boolean isGoogleLensFeatureEnabledOnUiThread(boolean isIncognito) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> LensUtils.isGoogleLensFeatureEnabled(isIncognito));
    }

    private boolean isGoogleLensShoppingFeatureEnabledOnUiThread(boolean isIncognito) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> LensUtils.isGoogleLensShoppingFeatureEnabled(isIncognito));
    }

    private boolean shouldLogUkmOnUiThread(boolean isIncognito) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> LensUtils.shouldLogUkm(isIncognito));
    }
}
