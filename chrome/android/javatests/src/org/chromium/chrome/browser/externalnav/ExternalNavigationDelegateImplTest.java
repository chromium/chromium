// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.content.Intent;
import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.url.Origin;

/**
 * Instrumentation tests for {@link ExternalNavigationHandler}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags
        .Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
        @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
                ChromeFeatureList.AUTOFILL_ASSISTANT_CHROME_ENTRY})
        public class ExternalNavigationDelegateImplTest {
    private static final String AUTOFILL_ASSISTANT_INTENT_URL =
            "intent://www.example.com#Intent;scheme=https;"
            + "B.org.chromium.chrome.browser.autofill_assistant.ENABLED=true;"
            + "B.org.chromium.chrome.browser.autofill_assistant.START_IMMEDIATELY=true;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode("https://www.example.com") + ";end";
    private static final String[] SUPERVISOR_START_ACTIONS = {
            "com.google.android.instantapps.START", "com.google.android.instantapps.nmr1.INSTALL",
            "com.google.android.instantapps.nmr1.VIEW"};
    private static final boolean IS_GOOGLE_REFERRER = true;

    class ExternalNavigationDelegateImplForTesting extends ExternalNavigationDelegateImpl {
        public ExternalNavigationDelegateImplForTesting() {
            super(mActivityTestRule.getActivity().getActivityTab());
        }

        @Override
        protected void startAutofillAssistantWithIntent(
                Intent targetIntent, String browserFallbackUrl) {
            mWasAutofillAssistantStarted = true;
        }

        public boolean wasAutofillAssistantStarted() {
            return mWasAutofillAssistantStarted;
        }

        // Convenience for testing that reduces boilerplate in constructing arguments to the
        // production method that are common across tests.
        public boolean handleWithAutofillAssistant(
                ExternalNavigationParams params, boolean isGoogleReferrer) {
            Intent intent;
            try {
                intent = Intent.parseUri(AUTOFILL_ASSISTANT_INTENT_URL, Intent.URI_INTENT_SCHEME);
            } catch (Exception ex) {
                Assert.assertTrue(false);
                return false;
            }

            String fallbackUrl = "https://www.example.com";

            return handleWithAutofillAssistant(params, intent, fallbackUrl, isGoogleReferrer);
        }

        private boolean mWasAutofillAssistantStarted;
    }

    private static class MockOrigin extends Origin {};

    public void maybeSetAndGetRequestMetadata(ExternalNavigationDelegateImpl delegate,
            Intent intent, boolean hasUserGesture, boolean isRendererInitiated,
            Origin initiatorOrigin) {
        delegate.maybeSetRequestMetadata(
                intent, hasUserGesture, isRendererInitiated, initiatorOrigin);
        IntentWithRequestMetadataHandler.RequestMetadata metadata =
                IntentWithRequestMetadataHandler.getInstance().getRequestMetadataAndClear(intent);
        Assert.assertEquals(hasUserGesture, metadata.hasUserGesture());
        Assert.assertEquals(isRendererInitiated, metadata.isRendererInitiated());
        Assert.assertEquals(initiatorOrigin, metadata.getInitiatorOrigin());
    }

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Test
    @SmallTest
    public void testMaybeSetPendingIncognitoUrl() {
        ExternalNavigationDelegateImpl delegate = new ExternalNavigationDelegateImpl(
                mActivityTestRule.getActivity().getActivityTab());

        String url = "http://www.example.com";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        delegate.maybeSetPendingIncognitoUrl(intent);

        Assert.assertTrue(
                intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));
        Assert.assertEquals(url, IntentHandler.getPendingIncognitoUrl());
    }

    @Test
    @SmallTest
    public void testIsIntentToInstantApp() {
        ExternalNavigationDelegateImpl delegate = new ExternalNavigationDelegateImpl(
                mActivityTestRule.getActivity().getActivityTab());

        // Check that the delegate correctly distinguishes instant app intents from others.
        String vanillaUrl = "http://www.example.com";
        Intent vanillaIntent = new Intent(Intent.ACTION_VIEW);
        vanillaIntent.setData(Uri.parse(vanillaUrl));

        String instantAppIntentUrl = "intent://buzzfeed.com/tasty#Intent;scheme=http;"
                + "package=com.google.android.instantapps.supervisor;"
                + "action=com.google.android.instantapps.START;"
                + "S.com.google.android.instantapps.FALLBACK_PACKAGE="
                + "com.android.chrome;S.com.google.android.instantapps.INSTANT_APP_PACKAGE="
                + "com.yelp.android;S.android.intent.extra.REFERRER_NAME="
                + "https%3A%2F%2Fwww.google.com;end";
        Intent instantAppIntent;
        try {
            instantAppIntent = Intent.parseUri(instantAppIntentUrl, Intent.URI_INTENT_SCHEME);
        } catch (Exception ex) {
            Assert.assertTrue(false);
            return;
        }

        Assert.assertFalse(delegate.isIntentToInstantApp(vanillaIntent));
        Assert.assertTrue(delegate.isIntentToInstantApp(instantAppIntent));

        // Check that Supervisor is detected by action even without package.
        for (String action : SUPERVISOR_START_ACTIONS) {
            String intentWithoutPackageUrl = "intent://buzzfeed.com/tasty#Intent;scheme=http;"
                    + "action=" + action + ";"
                    + "S.com.google.android.instantapps.FALLBACK_PACKAGE="
                    + "com.android.chrome;S.com.google.android.instantapps.INSTANT_APP_PACKAGE="
                    + "com.yelp.android;S.android.intent.extra.REFERRER_NAME="
                    + "https%3A%2F%2Fwww.google.com;end";
            try {
                instantAppIntent =
                        Intent.parseUri(intentWithoutPackageUrl, Intent.URI_INTENT_SCHEME);
            } catch (Exception ex) {
                Assert.assertTrue(false);
                return;
            }
            Assert.assertTrue(delegate.isIntentToInstantApp(instantAppIntent));
        }
    }

    @Test
    @SmallTest
    public void testMaybeAdjustInstantAppExtras() {
        ExternalNavigationDelegateImpl delegate = new ExternalNavigationDelegateImpl(
                mActivityTestRule.getActivity().getActivityTab());

        String url = "http://www.example.com";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        delegate.maybeAdjustInstantAppExtras(intent, /*isIntentToInstantApp=*/true);
        Assert.assertTrue(intent.hasExtra(InstantAppsHandler.IS_GOOGLE_SEARCH_REFERRER));

        delegate.maybeAdjustInstantAppExtras(intent, /*isIntentToInstantApp=*/false);
        Assert.assertFalse(intent.hasExtra(InstantAppsHandler.IS_GOOGLE_SEARCH_REFERRER));
    }

    @Test
    @SmallTest
    public void maybeSetRequestMetadata() {
        ExternalNavigationDelegateImpl delegate = new ExternalNavigationDelegateImpl(
                mActivityTestRule.getActivity().getActivityTab());

        String url = "http://www.example.com";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        delegate.maybeSetRequestMetadata(intent, false, false, null);
        Assert.assertNull(
                IntentWithRequestMetadataHandler.getInstance().getRequestMetadataAndClear(intent));

        maybeSetAndGetRequestMetadata(delegate, intent, true, true, null);
        maybeSetAndGetRequestMetadata(delegate, intent, true, false, null);
        maybeSetAndGetRequestMetadata(delegate, intent, false, true, null);
        maybeSetAndGetRequestMetadata(delegate, intent, false, false, new MockOrigin());
    }

    @Test
    @SmallTest
    public void testMaybeSetPendingReferrer() {
        ExternalNavigationDelegateImpl delegate = new ExternalNavigationDelegateImpl(
                mActivityTestRule.getActivity().getActivityTab());

        String url = "http://www.example.com";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        String referrerUrl = "http://www.example-referrer.com";
        delegate.maybeSetPendingReferrer(intent, referrerUrl);

        Assert.assertEquals(
                Uri.parse(referrerUrl), intent.getParcelableExtra(Intent.EXTRA_REFERRER));
        Assert.assertEquals(1, intent.getIntExtra(IntentHandler.EXTRA_REFERRER_ID, 0));
        Assert.assertEquals(referrerUrl, IntentHandler.getPendingReferrerUrl(1));
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_CHROME_ENTRY})
    public void
    testHandleWithAutofillAssistant_TriggersFromSearch() {
        ExternalNavigationDelegateImplForTesting delegate =
                new ExternalNavigationDelegateImplForTesting();

        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(AUTOFILL_ASSISTANT_INTENT_URL, /*isIncognito=*/false)
                        .build();

        Assert.assertTrue(delegate.handleWithAutofillAssistant(params, IS_GOOGLE_REFERRER));
        Assert.assertTrue(delegate.wasAutofillAssistantStarted());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_CHROME_ENTRY})
    public void
    testHandleWithAutofillAssistant_DoesNotTriggerFromSearchInIncognito() {
        ExternalNavigationDelegateImplForTesting delegate =
                new ExternalNavigationDelegateImplForTesting();

        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(AUTOFILL_ASSISTANT_INTENT_URL, /*isIncognito=*/true)
                        .build();

        Assert.assertFalse(delegate.handleWithAutofillAssistant(params, IS_GOOGLE_REFERRER));
        Assert.assertFalse(delegate.wasAutofillAssistantStarted());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_CHROME_ENTRY})
    public void
    testHandleWithAutofillAssistant_DoesNotTriggerFromDifferentOrigin() {
        ExternalNavigationDelegateImplForTesting delegate =
                new ExternalNavigationDelegateImplForTesting();

        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(AUTOFILL_ASSISTANT_INTENT_URL, /*isIncognito=*/false)
                        .build();

        Assert.assertFalse(delegate.handleWithAutofillAssistant(params, !IS_GOOGLE_REFERRER));
        Assert.assertFalse(delegate.wasAutofillAssistantStarted());
    }

    @Test
    @SmallTest
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_CHROME_ENTRY})
    public void
    testHandleWithAutofillAssistant_DoesNotTriggerWhenFeatureDisabled() {
        ExternalNavigationDelegateImplForTesting delegate =
                new ExternalNavigationDelegateImplForTesting();

        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(AUTOFILL_ASSISTANT_INTENT_URL, /*isIncognito=*/false)
                        .build();

        Assert.assertFalse(delegate.handleWithAutofillAssistant(params, IS_GOOGLE_REFERRER));
        Assert.assertFalse(delegate.wasAutofillAssistantStarted());
    }
}
