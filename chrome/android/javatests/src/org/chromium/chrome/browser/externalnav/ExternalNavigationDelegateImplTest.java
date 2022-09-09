// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import static org.mockito.Mockito.doReturn;

import android.content.Intent;
import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Function;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.autofill_assistant.AssistantFeatures;
import org.chromium.components.external_intents.ExternalNavigationDelegate.IntentToAutofillAllowingAppResult;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/**
 * Instrumentation tests for {@link ExternalNavigationHandler}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.DisableFeatures({AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
        AssistantFeatures.AUTOFILL_ASSISTANT_CHROME_ENTRY_NAME})
public class ExternalNavigationDelegateImplTest {
    private static final String AUTOFILL_ASSISTANT_INTENT_URL =
            "intent://www.example.com#Intent;scheme=https;"
            + "B.org.chromium.chrome.browser.autofill_assistant.ENABLED=true;"
            + "B.org.chromium.chrome.browser.autofill_assistant.START_IMMEDIATELY=true;"
            + "S.org.chromium.chrome.browser.autofill_assistant.ORIGINAL_DEEPLINK="
            + Uri.encode("https://www.example.com") + ";"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode("https://www.example.com") + ";end";
    private static final String AUTOFILL_ASSISTANT_APP_OVERRIDE_INTENT_URL =
            "intent://www.example.com#Intent;scheme=https;"
            + "B.org.chromium.chrome.browser.autofill_assistant.ENABLED=true;"
            + "B.org.chromium.chrome.browser.autofill_assistant.START_IMMEDIATELY=true;"
            + "S.org.chromium.chrome.browser.autofill_assistant.ALLOW_APP=true;"
            + "S.org.chromium.chrome.browser.autofill_assistant.ORIGINAL_DEEPLINK="
            + Uri.encode("https://www.example.com") + ";"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode("https://www.example.com") + ";end";
    private static final String[] SUPERVISOR_START_ACTIONS = {
            "com.google.android.instantapps.START", "com.google.android.instantapps.nmr1.INSTALL",
            "com.google.android.instantapps.nmr1.VIEW"};
    private static final boolean IS_GOOGLE_REFERRER = true;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    class ExternalNavigationDelegateImplForTesting extends ExternalNavigationDelegateImpl {
        private boolean mWasAutofillAssistantStarted;
        private @IntentToAutofillAllowingAppResult int mAutofillAssistantAppOverrideResult;
        private Function<Intent, Boolean> mCanExternalAppHandleIntent;

        public ExternalNavigationDelegateImplForTesting(Tab activityTab) {
            super(activityTab);
        }

        @Override
        protected void startAutofillAssistantWithIntent(
                Intent targetIntent, GURL browserFallbackUrl) {
            mWasAutofillAssistantStarted = true;
        }

        public boolean wasAutofillAssistantStarted() {
            return mWasAutofillAssistantStarted;
        }

        public @IntentToAutofillAllowingAppResult int getAutofillAssistantAppOverrideResult() {
            return mAutofillAssistantAppOverrideResult;
        }

        public void setCanExternalAppHandleIntent(Function<Intent, Boolean> value) {
            mCanExternalAppHandleIntent = value;
        }

        // Convenience for testing that reduces boilerplate in constructing arguments to the
        // production method that are common across tests.
        public boolean handleWithAutofillAssistant(
                ExternalNavigationParams params, boolean isGoogleReferrer) {
            Intent intent;
            try {
                intent = Intent.parseUri(params.getUrl().getSpec(), Intent.URI_INTENT_SCHEME);
            } catch (Exception ex) {
                Assert.assertTrue(false);
                return false;
            }

            GURL fallbackUrl = new GURL("https://www.example.com");

            mAutofillAssistantAppOverrideResult = isIntentToAutofillAssistantAllowingApp(
                    params, intent, mCanExternalAppHandleIntent);
            return handleWithAutofillAssistant(params, intent, fallbackUrl, isGoogleReferrer);
        }
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

    private ExternalNavigationDelegateImpl mExternalNavigationDelegateImpl;
    private ExternalNavigationDelegateImplForTesting mExternalNavigationDelegateImplForTesting;

    @Mock
    Tab mMockTab;
    @Mock
    WindowAndroid mMockWindowAndroid;

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        doReturn(mMockWindowAndroid).when(mMockTab).getWindowAndroid();
        mExternalNavigationDelegateImpl = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> new ExternalNavigationDelegateImpl(mMockTab));
        mExternalNavigationDelegateImplForTesting =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> new ExternalNavigationDelegateImplForTesting(mMockTab));
    }

    @Test
    @SmallTest
    public void testMaybeSetPendingIncognitoUrl() {
        String url = "http://www.example.com";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        mExternalNavigationDelegateImpl.maybeSetPendingIncognitoUrl(intent);

        Assert.assertTrue(
                intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));
        Assert.assertEquals(url, IntentHandler.getPendingIncognitoUrl());
    }

    @Test
    @SmallTest
    public void testMaybeAdjustInstantAppExtras() {
        String url = "http://www.example.com";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        mExternalNavigationDelegateImpl.maybeAdjustInstantAppExtras(
                intent, /*isIntentToInstantApp=*/true);
        Assert.assertTrue(intent.hasExtra(InstantAppsHandler.IS_GOOGLE_SEARCH_REFERRER));

        mExternalNavigationDelegateImpl.maybeAdjustInstantAppExtras(
                intent, /*isIntentToInstantApp=*/false);
        Assert.assertFalse(intent.hasExtra(InstantAppsHandler.IS_GOOGLE_SEARCH_REFERRER));
    }

    @Test
    @SmallTest
    public void maybeSetRequestMetadata() {
        String url = "http://www.example.com";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        mExternalNavigationDelegateImpl.maybeSetRequestMetadata(intent, false, false, null);
        Assert.assertNull(
                IntentWithRequestMetadataHandler.getInstance().getRequestMetadataAndClear(intent));

        maybeSetAndGetRequestMetadata(mExternalNavigationDelegateImpl, intent, true, true, null);
        maybeSetAndGetRequestMetadata(mExternalNavigationDelegateImpl, intent, true, false, null);
        maybeSetAndGetRequestMetadata(mExternalNavigationDelegateImpl, intent, false, true, null);
        maybeSetAndGetRequestMetadata(
                mExternalNavigationDelegateImpl, intent, false, false, new MockOrigin());
    }

    @Test
    @SmallTest
    public void testMaybeSetPendingReferrer() {
        String url = "http://www.example.com/";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        String referrerUrl = "http://www.example-referrer.com/";
        mExternalNavigationDelegateImpl.maybeSetPendingReferrer(intent, new GURL(referrerUrl));

        Assert.assertEquals(
                Uri.parse(referrerUrl), intent.getParcelableExtra(Intent.EXTRA_REFERRER));
        Assert.assertEquals(1, intent.getIntExtra(IntentHandler.EXTRA_REFERRER_ID, 0));
        Assert.assertEquals(referrerUrl, IntentHandler.getPendingReferrerUrl(1));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_CHROME_ENTRY_NAME})
    public void
    testHandleWithAutofillAssistant_TriggersFromSearch() {
        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(new GURL(AUTOFILL_ASSISTANT_INTENT_URL), /*isIncognito=*/false)
                        .build();

        Assert.assertTrue(mExternalNavigationDelegateImplForTesting.handleWithAutofillAssistant(
                params, IS_GOOGLE_REFERRER));
        Assert.assertTrue(mExternalNavigationDelegateImplForTesting.wasAutofillAssistantStarted());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_CHROME_ENTRY_NAME})
    public void
    testHandleWithAutofillAssistant_DoesNotTriggerFromSearchInIncognito() {
        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(new GURL(AUTOFILL_ASSISTANT_INTENT_URL), /*isIncognito=*/true)
                        .build();

        Assert.assertFalse(mExternalNavigationDelegateImplForTesting.handleWithAutofillAssistant(
                params, IS_GOOGLE_REFERRER));
        Assert.assertFalse(mExternalNavigationDelegateImplForTesting.wasAutofillAssistantStarted());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_CHROME_ENTRY_NAME})
    public void
    testHandleWithAutofillAssistant_DoesNotTriggerFromDifferentOrigin() {
        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(new GURL(AUTOFILL_ASSISTANT_INTENT_URL), /*isIncognito=*/false)
                        .build();

        Assert.assertFalse(mExternalNavigationDelegateImplForTesting.handleWithAutofillAssistant(
                params, !IS_GOOGLE_REFERRER));
        Assert.assertFalse(mExternalNavigationDelegateImplForTesting.wasAutofillAssistantStarted());
    }

    @Test
    @SmallTest
    @Features.DisableFeatures({AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_CHROME_ENTRY_NAME})
    public void
    testHandleWithAutofillAssistant_DoesNotTriggerWhenFeatureDisabled() {
        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(new GURL(AUTOFILL_ASSISTANT_INTENT_URL), /*isIncognito=*/false)
                        .build();

        Assert.assertFalse(mExternalNavigationDelegateImplForTesting.handleWithAutofillAssistant(
                params, IS_GOOGLE_REFERRER));
        Assert.assertFalse(mExternalNavigationDelegateImplForTesting.wasAutofillAssistantStarted());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_CHROME_ENTRY_NAME})
    public void
    testHandleWithAutofillAssistant_DoesNotAllowAppOverrideIfNotSpecified() {
        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(new GURL(AUTOFILL_ASSISTANT_INTENT_URL), /*isIncognito=*/false)
                        .build();

        mExternalNavigationDelegateImplForTesting.setCanExternalAppHandleIntent((i) -> true);

        Assert.assertTrue(mExternalNavigationDelegateImplForTesting.handleWithAutofillAssistant(
                params, IS_GOOGLE_REFERRER));
        Assert.assertEquals(IntentToAutofillAllowingAppResult.NONE,
                mExternalNavigationDelegateImplForTesting.getAutofillAssistantAppOverrideResult());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_CHROME_ENTRY_NAME})
    public void
    testHandleWithAutofillAssistant_AllowAppOverrideIfSpecified() {
        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(new GURL(AUTOFILL_ASSISTANT_APP_OVERRIDE_INTENT_URL),
                                /*isIncognito=*/false)
                        .build();

        mExternalNavigationDelegateImplForTesting.setCanExternalAppHandleIntent((i) -> true);

        Assert.assertTrue(mExternalNavigationDelegateImplForTesting.handleWithAutofillAssistant(
                params, IS_GOOGLE_REFERRER));
        Assert.assertEquals(IntentToAutofillAllowingAppResult.DEFER_TO_APP_NOW,
                mExternalNavigationDelegateImplForTesting.getAutofillAssistantAppOverrideResult());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_CHROME_ENTRY_NAME})
    public void
    testHandleWithAutofillAssistant_DoesNotAllowAppOverrideIfSpecifiedInIncognito() {
        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(new GURL(AUTOFILL_ASSISTANT_APP_OVERRIDE_INTENT_URL),
                                /*isIncognito=*/true)
                        .build();

        mExternalNavigationDelegateImplForTesting.setCanExternalAppHandleIntent((i) -> true);

        Assert.assertFalse(mExternalNavigationDelegateImplForTesting.handleWithAutofillAssistant(
                params, IS_GOOGLE_REFERRER));
        Assert.assertEquals(IntentToAutofillAllowingAppResult.NONE,
                mExternalNavigationDelegateImplForTesting.getAutofillAssistantAppOverrideResult());
    }
}
