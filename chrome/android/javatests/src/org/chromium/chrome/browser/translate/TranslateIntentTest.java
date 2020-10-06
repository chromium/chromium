// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.TranslateCompactInfoBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the "ACTION_TRANSLATE_TAB" intent.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TranslateIntentTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();
    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String TRANSLATE_PAGE = "/chrome/test/data/translate/fr_test.html";
    private static final String NON_TRANSLATE_PAGE =
            "/chrome/test/data/translate/english_page.html";

    private InfoBarContainer mInfoBarContainer;
    private InfoBarTestAnimationListener mListener;

    /**
     * Wait until the activity tab is translatable. This is useful in cases where we can't wait on
     * the infobar.
     */
    private void waitUntilTranslatable() {
        CriteriaHelper.pollInstrumentationThread(() -> {
            Tab tab = sActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(tab, Matchers.notNullValue());
            boolean canManuallyTranslate = false;
            try {
                canManuallyTranslate = TestThreadUtils.runOnUiThreadBlocking(
                        () -> TranslateBridge.canManuallyTranslate(tab));
            } catch (ExecutionException e) {
                e.printStackTrace();
                canManuallyTranslate = false;
            }
            Criteria.checkThat(canManuallyTranslate, Matchers.is(true));
        });
    }

    /**
     * Wait until the given translate infobar is in the given state.
     */
    private void waitForTranslateInfoBarState(boolean expectTranslated) {
        TranslateCompactInfoBar infoBar =
                (TranslateCompactInfoBar) mInfoBarContainer.getInfoBarsForTesting().get(0);

        CriteriaHelper.pollInstrumentationThread(() -> {
            if (expectTranslated) {
                return !infoBar.isSourceTabSelectedForTesting()
                        && infoBar.isTargetTabSelectedForTesting();
            } else {
                return infoBar.isSourceTabSelectedForTesting()
                        && !infoBar.isTargetTabSelectedForTesting();
            }
        });
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
        IntentHandler.addTrustedIntentExtras(intent);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getActivity().onNewIntent(intent));
    }

    @Before
    public void setUp() {
        mInfoBarContainer = sActivityTestRule.getInfoBarContainer();
        mListener = new InfoBarTestAnimationListener();
        mInfoBarContainer.addAnimationListener(mListener);
    }

    @After
    public void tearDown() {
        mInfoBarContainer.removeAnimationListener(mListener);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1127786 and https://crbug.com/1135682")
    @Features.DisableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    public void
    testTranslateIntentDisabled() throws TimeoutException {
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        // Load a page that triggers the translate recommendation.
        sActivityTestRule.loadUrl(url);
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");
        waitForTranslateInfoBarState(false);

        sendTranslateIntent(url, null);

        // No translation occurs.
        waitForTranslateInfoBarState(false);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1127786 and https://crbug.com/1135682")
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    public void
    testTranslateIntentOnTranslatePage() throws TimeoutException {
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        // Load a page that triggers the translate recommendation.
        sActivityTestRule.loadUrl(url);
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");
        waitForTranslateInfoBarState(false);

        sendTranslateIntent(url, null);

        // Only the target tab is selected.
        waitForTranslateInfoBarState(true);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1127786 and https://crbug.com/1135682")
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    public void
    testTranslateIntentOnNonTranslatePage() throws TimeoutException {
        final String url = sActivityTestRule.getTestServer().getURL(NON_TRANSLATE_PAGE);
        // Load a page that doesn't trigger the translate recommendation.
        sActivityTestRule.loadUrl(url);

        waitUntilTranslatable();
        Assert.assertTrue(mInfoBarContainer.getInfoBarsForTesting().isEmpty());

        sendTranslateIntent(url, null);
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");

        // Only the target tab is selected.
        waitForTranslateInfoBarState(true);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1127786 and https://crbug.com/1135682")
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    public void
    testTranslateIntentWithTargetLanguage() throws TimeoutException, ExecutionException {
        final String url = sActivityTestRule.getTestServer().getURL(NON_TRANSLATE_PAGE);
        // Load a page that doesn't trigger the translate recommendation.
        sActivityTestRule.loadUrl(url);

        waitUntilTranslatable();
        Assert.assertTrue(mInfoBarContainer.getInfoBarsForTesting().isEmpty());

        List<String> acceptCodes =
                TestThreadUtils.runOnUiThreadBlocking(() -> TranslateBridge.getUserLanguageCodes());
        Assert.assertFalse(acceptCodes.contains("de"));

        sendTranslateIntent(url, "de");
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");

        // Only the target tab is selected.
        waitForTranslateInfoBarState(true);

        // Verify that German has been added to the user's accept languages.
        acceptCodes =
                TestThreadUtils.runOnUiThreadBlocking(() -> TranslateBridge.getUserLanguageCodes());
        Assert.assertTrue(acceptCodes.contains("de"));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1127786 and https://crbug.com/1135682")
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    public void
    testTranslateIntentWithUnsupportedTargetLanguage() throws TimeoutException {
        final String url = sActivityTestRule.getTestServer().getURL(NON_TRANSLATE_PAGE);
        // Load a page that doesn't trigger the translate recommendation.
        sActivityTestRule.loadUrl(url);

        waitUntilTranslatable();
        Assert.assertTrue(mInfoBarContainer.getInfoBarsForTesting().isEmpty());

        sendTranslateIntent(url, "unsupported");

        Assert.assertTrue(mInfoBarContainer.getInfoBarsForTesting().isEmpty());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1127786 and https://crbug.com/1135682")
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    public void
    testTranslateIntentOnIncognito() throws TimeoutException {
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        // Load a page that triggers the translate recommendation.
        sActivityTestRule.loadUrlInNewTab(url, /*incognito=*/true);
        // A new tab is opened for this test so we need to get the new container.
        mInfoBarContainer = sActivityTestRule.getInfoBarContainer();
        InfoBarTestAnimationListener mListener = new InfoBarTestAnimationListener();
        mInfoBarContainer.addAnimationListener(mListener);
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");
        waitForTranslateInfoBarState(false);

        sendTranslateIntent(url, null);

        // No translation should happen.
        waitForTranslateInfoBarState(false);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1127786 and https://crbug.com/1135682")
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    public void
    testTranslateIntentWithUrlMismatch() throws TimeoutException {
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        // Load a page that triggers the translate recommendation.
        sActivityTestRule.loadUrl(url);
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");
        waitForTranslateInfoBarState(false);

        sendTranslateIntent("http://incorrect.com", null);

        // No translation should happen.
        waitForTranslateInfoBarState(false);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1127786 and https://crbug.com/1135682")
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    public void
    testTranslateIntentWithoutExpectedUrl() throws TimeoutException {
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        // Load a page that triggers the translate recommendation.
        sActivityTestRule.loadUrl(url);
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");
        waitForTranslateInfoBarState(false);

        sendTranslateIntent(null, null);

        // No translation should happen.
        waitForTranslateInfoBarState(false);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1127786 and https://crbug.com/1135682")
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    public void
    testTranslateIntentVerifyComponent() throws TimeoutException {
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        // Load a page that triggers the translate recommendation.
        sActivityTestRule.loadUrl(url);
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");
        waitForTranslateInfoBarState(false);

        Context context = ContextUtils.getApplicationContext();

        final Intent intent = new Intent(TranslateIntentHandler.ACTION_TRANSLATE_TAB);
        intent.setClassName(context, "com.google.android.apps.chrome.TranslateDispatcher");
        intent.putExtra(TranslateIntentHandler.EXTRA_EXPECTED_URL, url);
        // Send this via startActivity so we don't bypass LaunchIntentHandler.
        TestThreadUtils.runOnUiThreadBlocking(() -> context.startActivity(intent));

        // Only the target tab is selected.
        waitForTranslateInfoBarState(true);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1127786 and https://crbug.com/1135682")
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_INTENT})
    public void
    testTranslateIntentIncorrectComponent() throws TimeoutException {
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        // Load a page that triggers the translate recommendation.
        sActivityTestRule.loadUrl(url);
        mListener.addInfoBarAnimationFinished("InfoBar not opened.");
        waitForTranslateInfoBarState(false);

        Context context = ContextUtils.getApplicationContext();

        final Intent intent = new Intent(TranslateIntentHandler.ACTION_TRANSLATE_TAB);
        // Bypassing the TranslateDispatcher activity alias should reject the intent.
        intent.setClass(context, ChromeLauncherActivity.class);
        intent.putExtra(TranslateIntentHandler.EXTRA_EXPECTED_URL, url);
        TestThreadUtils.runOnUiThreadBlocking(() -> context.startActivity(intent));

        // No translation should happen.
        waitForTranslateInfoBarState(false);
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
                component.getClassName(), TranslateIntentHandler.COMPONENT_TRANSLATE_DISPATCHER);
    }
}
