// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.share.LensUtils.IntentType;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests of {@link LensUtils}.
 * TODO(https://crbug.com/1054738): Reimplement LensUtilsTest as robolectric tests
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class LensUtilsTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when user is signed
     * in.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentSignedInTest() {
        mBrowserTestRule.addAndSignInTestAccount();

        Intent intentNoUri = getShareWithGoogleLensIntentOnUiThread(Uri.EMPTY,
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
        Assert.assertEquals("Intent without image has incorrect URI", "googleapp://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "test%40gmail.com&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
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
        mBrowserTestRule.addAndSignInTestAccount();

        Intent intentNoUri = getShareWithGoogleLensIntentOnUiThread(Uri.EMPTY,
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
        Assert.assertEquals("Intent without image has incorrect URI", "google://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
        Assert.assertEquals("Intent with image has incorrect URI",
                "google://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url"
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
        mBrowserTestRule.addAndSignInTestAccount();
        Intent intentNoUri = getShareWithGoogleLensIntentOnUiThread(Uri.EMPTY,
                /* isIncognito= */ true, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
        Assert.assertEquals("Intent without image has incorrect URI", "googleapp://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ true, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
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
        mBrowserTestRule.addAndSignInTestAccount();

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "", /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
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
        mBrowserTestRule.addAndSignInTestAccount();

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ true, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "", /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
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
                /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
        Assert.assertEquals("Intent without image has incorrect URI", "googleapp://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "",
                /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
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
                        /* intentType= */ IntentType.DEFAULT,
                        /* requiresConfirmation= */ false);
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=0",
                intentWithContentUriZeroTimestamp.getData().toString());
    }

    private Intent getShareWithGoogleLensIntentOnUiThread(Uri imageUri, boolean isIncognito,
            long currentTimeNanos, String srcUrl, String titleOrAltText,
            @IntentType final int intentType, boolean requiresConfirmation) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> LensUtils.getShareWithGoogleLensIntent(imageUri, isIncognito,
                                currentTimeNanos, srcUrl, titleOrAltText, intentType,
                                requiresConfirmation));
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
                /* titleOrAltText */ "An image description.",
                /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
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
                /* titleOrAltText */ "An image description.",
                /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
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
                /* titleOrAltText */ "", /* intentType= */ IntentType.SHOPPING,
                /* requiresConfirmation= */ false);
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
                /* titleOrAltText */ "", /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ true);
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
                /* titleOrAltText */ "An image description.",
                /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
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
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when image src is available but
     * not enabled by finch.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentSrcDisabledNotAddedTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "http://www.google.com?key=val",
                /* titleOrAltText */ "", /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
        // The account name should not be included in the intent because the uesr is incognito.
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
                /* titleOrAltText */ "", /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234&ImageSrc="
                        + "http%3A%2F%2Fwww.google.com%3Fkey%3Dval",
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
                /* titleOrAltText */ "", /* intentType= */ IntentType.DEFAULT,
                /* requiresConfirmation= */ false);
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
        final String pageUrl = "shopping-site-2.com/product_1";
        assertTrue(isInShoppingAllowlistOnUiThread(pageUrl));
    }

    /**
     * Test {@link LensUtils#isInShoppingAllowlist(url)} method for url with shopping url patterns.
     */
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:shoppingUrlPatterns/^shopping-site.*"})
    @Test
    @SmallTest
    public void
    isInShoppingAllowlistWithShoppingUrlPatternsTest() {
        final String pageUrl = "shopping-site-2.com/product_1";
        assertTrue(isInShoppingAllowlistOnUiThread(pageUrl));
    }

    /**
     * Test {@link LensUtils#isInShoppingAllowlist(url)} method for url with default shopping url
     * patterns.
     */
    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST})
    public void isInShoppingAllowlistWithDefaultShoppingUrlPatternTest() {
        final String googleShoppingItemUrl = "https://www.google.com/shopping/product_1";
        final String googleShoppingPageUrl = "https://www.google.com/search?=8893t5/tbm=shop/dress";
        assertTrue(isInShoppingAllowlistOnUiThread(googleShoppingPageUrl));
        assertTrue(isInShoppingAllowlistOnUiThread(googleShoppingItemUrl));
    }

    private boolean isInShoppingAllowlistOnUiThread(String imageUri) {
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
}