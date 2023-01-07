// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.TranslateUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the "ACTION_TRANSLATE_TAB" intent.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(TranslateAssistContentTest.TRANSLATE_BATCH_NAME)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TranslateIntentTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String TRANSLATE_PAGE = "/chrome/test/data/translate/fr_test.html";
    private static final String NON_TRANSLATE_PAGE =
            "/chrome/test/data/translate/english_page.html";

    private InfoBarContainer mInfoBarContainer;

    /**
     * Returns true if a test that requires internet access should be skipped due to an
     * out-of-process NetworkService. When the NetworkService is run out-of-process, a fake DNS
     * resolver is used that will fail to resolve any non-local names. crbug.com/1134812 is tracking
     * the changes to make the translate service mockable and remove the internet requirement.
     */
    private boolean shouldSkipDueToNetworkService() {
        return !ChromeFeatureList.isEnabled("NetworkServiceInProcess2");
    }

    /**
     * Sends a TRANSLATE_TAB intent to the test activity with the given expectedUrl and, if
     * non-null, targetLanguageCode.
     */
    private void sendTranslateIntent(String expectedUrl, String targetLanguageCode) {
        final Intent intent = new Intent(TranslateIntentHandler.ACTION_TRANSLATE_TAB);
        if (expectedUrl != null) {
            intent.putExtra(TranslateIntentHandler.EXTRA_EXPECTED_URL, expectedUrl);
        }
        if (targetLanguageCode != null) {
            intent.putExtra(TranslateIntentHandler.EXTRA_TARGET_LANGUAGE_CODE, targetLanguageCode);
        }
        intent.setClassName(ContextUtils.getApplicationContext(),
                TranslateIntentHandler.COMPONENT_TRANSLATE_DISPATCHER);
        IntentUtils.addTrustedIntentExtras(intent);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getActivity().onNewIntent(intent));
    }

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> TranslateBridge.setIgnoreMissingKeyForTesting(true));
        mInfoBarContainer = sActivityTestRule.getInfoBarContainer();
    }

    @After
    public void tearDown() {
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.DisableFeatures({ChromeFeatureList.TRANSLATE_INTENT,
            ChromeFeatureList.TRANSLATE_TFLITE, ChromeFeatureList.TRANSLATE_MESSAGE_UI})
    public void
    testTranslateIntentDisabled() throws TimeoutException {
        if (shouldSkipDueToNetworkService()) return;
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        // Load a page that triggers the translate recommendation.
        sActivityTestRule.loadUrl(url);
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/false);

        sendTranslateIntent(url, null);

        // No translation occurs.
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/false);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    @Features.
    DisableFeatures({ChromeFeatureList.TRANSLATE_TFLITE, ChromeFeatureList.TRANSLATE_MESSAGE_UI})
    public void testTranslateIntentOnTranslatePage() throws TimeoutException {
        if (shouldSkipDueToNetworkService()) return;
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        // Load a page that triggers the translate recommendation.
        sActivityTestRule.loadUrl(url);
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/false);

        sendTranslateIntent(url, null);

        // Only the target tab is selected.
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/true);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    @Features.DisableFeatures({ChromeFeatureList.TRANSLATE_MESSAGE_UI})
    public void testTranslateIntentOnNonTranslatePage() throws TimeoutException {
        if (shouldSkipDueToNetworkService()) return;
        final String url = sActivityTestRule.getTestServer().getURL(NON_TRANSLATE_PAGE);
        // Load a page that doesn't trigger the translate recommendation.
        sActivityTestRule.loadUrl(url);

        TranslateUtil.waitUntilTranslatable(sActivityTestRule.getActivity().getActivityTab());
        Assert.assertTrue(mInfoBarContainer.getInfoBarsForTesting().isEmpty());

        sendTranslateIntent(url, null);

        // Only the target tab is selected.
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/true);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    @Features.DisableFeatures({ChromeFeatureList.TRANSLATE_MESSAGE_UI})
    public void testTranslateIntentWithTargetLanguage()
            throws TimeoutException, ExecutionException {
        if (shouldSkipDueToNetworkService()) return;
        final String url = sActivityTestRule.getTestServer().getURL(NON_TRANSLATE_PAGE);
        // Load a page that doesn't trigger the translate recommendation.
        sActivityTestRule.loadUrl(url);

        TranslateUtil.waitUntilTranslatable(sActivityTestRule.getActivity().getActivityTab());
        Assert.assertTrue(mInfoBarContainer.getInfoBarsForTesting().isEmpty());

        List<String> acceptCodes =
                TestThreadUtils.runOnUiThreadBlocking(() -> TranslateBridge.getUserLanguageCodes());
        Assert.assertFalse(acceptCodes.contains("de"));

        sendTranslateIntent(url, "de");

        // Only the target tab is selected.
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/true);

        // Verify that German has been added to the user's accept languages.
        acceptCodes =
                TestThreadUtils.runOnUiThreadBlocking(() -> TranslateBridge.getUserLanguageCodes());
        Assert.assertTrue(acceptCodes.contains("de"));
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    @Features.DisableFeatures({ChromeFeatureList.TRANSLATE_MESSAGE_UI})
    public void testTranslateIntentWithIdenticalSourceAndTarget()
            throws TimeoutException, ExecutionException {
        if (shouldSkipDueToNetworkService()) return;
        final String url = sActivityTestRule.getTestServer().getURL(NON_TRANSLATE_PAGE);
        // Load a page that doesn't trigger the translate recommendation.
        sActivityTestRule.loadUrl(url);

        TranslateUtil.waitUntilTranslatable(sActivityTestRule.getActivity().getActivityTab());
        Assert.assertTrue(mInfoBarContainer.getInfoBarsForTesting().isEmpty());

        sendTranslateIntent(url, "en");

        // Only the target tab is selected.
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/true);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    @Features.DisableFeatures({ChromeFeatureList.TRANSLATE_MESSAGE_UI})
    public void testTranslateIntentWithUnsupportedTargetLanguage() throws TimeoutException {
        if (shouldSkipDueToNetworkService()) return;
        final String url = sActivityTestRule.getTestServer().getURL(NON_TRANSLATE_PAGE);
        // Load a page that doesn't trigger the translate recommendation.
        sActivityTestRule.loadUrl(url);

        TranslateUtil.waitUntilTranslatable(sActivityTestRule.getActivity().getActivityTab());
        Assert.assertTrue(mInfoBarContainer.getInfoBarsForTesting().isEmpty());

        sendTranslateIntent(url, "unsupported");

        // Infobar should be shown but not translated.
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/false);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    @Features.
    DisableFeatures({ChromeFeatureList.TRANSLATE_TFLITE, ChromeFeatureList.TRANSLATE_MESSAGE_UI})
    public void testTranslateIntentOnIncognito() throws TimeoutException {
        if (shouldSkipDueToNetworkService()) return;
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        // Load a page that triggers the translate recommendation.
        sActivityTestRule.loadUrlInNewTab(url, /*incognito=*/true);
        // A new tab is opened for this test so we need to get the new container.
        mInfoBarContainer = sActivityTestRule.getInfoBarContainer();
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/false);

        sendTranslateIntent(url, null);

        // No translation should happen.
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/false);

        // We opened a new tab for this test. Ensure that it's closed.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { sActivityTestRule.getActivity().getTabModelSelector().closeAllTabs(); });
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    @Features.
    DisableFeatures({ChromeFeatureList.TRANSLATE_TFLITE, ChromeFeatureList.TRANSLATE_MESSAGE_UI})
    public void testTranslateIntentWithUrlMismatch() throws TimeoutException {
        if (shouldSkipDueToNetworkService()) return;
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        // Load a page that triggers the translate recommendation.
        sActivityTestRule.loadUrl(url);
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/false);

        sendTranslateIntent("http://incorrect.com", null);

        // No translation should happen.
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/false);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    @Features.
    DisableFeatures({ChromeFeatureList.TRANSLATE_TFLITE, ChromeFeatureList.TRANSLATE_MESSAGE_UI})
    public void testTranslateIntentWithoutExpectedUrl() throws TimeoutException {
        if (shouldSkipDueToNetworkService()) return;
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        // Load a page that triggers the translate recommendation.
        sActivityTestRule.loadUrl(url);
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/false);

        sendTranslateIntent(null, null);

        // No translation should happen.
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/false);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    @Features.
    DisableFeatures({ChromeFeatureList.TRANSLATE_TFLITE, ChromeFeatureList.TRANSLATE_MESSAGE_UI})
    public void testTranslateIntentVerifyComponent() throws TimeoutException {
        if (shouldSkipDueToNetworkService()) return;
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        // Load a page that triggers the translate recommendation.
        sActivityTestRule.loadUrl(url);
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/false);

        Context context = ContextUtils.getApplicationContext();

        final Intent intent = new Intent(TranslateIntentHandler.ACTION_TRANSLATE_TAB);
        intent.setClassName(context, TranslateIntentHandler.COMPONENT_TRANSLATE_DISPATCHER);
        intent.putExtra(TranslateIntentHandler.EXTRA_EXPECTED_URL, url);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // The Activity is already launched so we cannot use startActivityCompletely().
        sActivityTestRule.getActivity().startActivity(intent);

        // Only the target tab is selected.
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/true);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    @Features.
    DisableFeatures({ChromeFeatureList.TRANSLATE_TFLITE, ChromeFeatureList.TRANSLATE_MESSAGE_UI})
    public void testTranslateIntentIncorrectComponent() throws TimeoutException {
        if (shouldSkipDueToNetworkService()) return;
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        // Load a page that triggers the translate recommendation.
        sActivityTestRule.loadUrl(url);
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/false);

        Context context = ContextUtils.getApplicationContext();

        final Intent intent = new Intent(TranslateIntentHandler.ACTION_TRANSLATE_TAB);
        // Bypassing the TranslateDispatcher activity alias should reject the intent.
        intent.setClass(context, ChromeLauncherActivity.class);
        intent.putExtra(TranslateIntentHandler.EXTRA_EXPECTED_URL, url);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        TestThreadUtils.runOnUiThreadBlocking(() -> context.startActivity(intent));

        // No translation should happen.
        TranslateUtil.waitForTranslateInfoBarState(mInfoBarContainer, /*expectTranslated=*/false);
    }

    @Test
    @SmallTest
    public void testTranslateIntentFilter() {
        Context context = ContextUtils.getApplicationContext();
        PackageManager pm = context.getPackageManager();
        final Intent intent = new Intent(TranslateIntentHandler.ACTION_TRANSLATE_TAB);
        intent.setPackage(context.getPackageName());
        final ComponentName component = intent.resolveActivity(pm);
        Assert.assertEquals(
                TranslateIntentHandler.COMPONENT_TRANSLATE_DISPATCHER, component.getClassName());
    }
}
