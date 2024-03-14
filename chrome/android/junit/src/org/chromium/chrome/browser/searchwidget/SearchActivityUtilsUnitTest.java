// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.text.TextUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient.SearchType;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityConstants;
import org.chromium.url.GURL;

@RunWith(BaseRobolectricTestRunner.class)
public class SearchActivityUtilsUnitTest {
    // Dummy Activity class that guarantees the PackageName is valid for IntentUtils.
    private static class TestActivity extends Activity {}

    private static final GURL GOOD_URL = new GURL("https://abc.xyz");
    private static final GURL EMPTY_URL = GURL.emptyGURL();
    private Activity mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();

    private Intent buildSearchWidgetIntent() {
        Intent intent = new Intent();
        intent.putExtra(SearchWidgetProvider.EXTRA_FROM_SEARCH_WIDGET, true);
        return intent;
    }

    @Test
    public void createIntent_forTextSearch() {
        @IntentOrigin
        int[] origins =
                new int[] {
                    IntentOrigin.SEARCH_WIDGET,
                    IntentOrigin.QUICK_ACTION_SEARCH_WIDGET,
                    IntentOrigin.CUSTOM_TAB,
                };

        SearchActivityClient client = new SearchActivityUtils();
        for (int origin : origins) {
            // null URL
            var intent = client.createIntent(mActivity, origin, null, SearchType.TEXT);
            assertEquals(SearchActivityUtils.ACTION_TEXT_SEARCH, intent.getAction());
            assertNull(intent.getStringExtra(SearchActivityUtils.EXTRA_CURRENT_URL));
            assertEquals(SearchType.TEXT, SearchActivityUtils.getIntentSearchType(intent));
            assertEquals(origin, SearchActivityUtils.getIntentOrigin(intent));

            // non-null URL
            intent =
                    client.createIntent(
                            mActivity, origin, new GURL("http://abc.xyz"), SearchType.TEXT);
            assertEquals(SearchActivityUtils.ACTION_TEXT_SEARCH, intent.getAction());
            assertEquals(
                    "http://abc.xyz/",
                    intent.getStringExtra(SearchActivityUtils.EXTRA_CURRENT_URL));
            assertEquals(SearchType.TEXT, SearchActivityUtils.getIntentSearchType(intent));
            assertEquals(origin, SearchActivityUtils.getIntentOrigin(intent));
        }
    }

    @Test
    public void createIntent_forVoiceSearch() {
        @IntentOrigin
        int[] origins =
                new int[] {
                    IntentOrigin.SEARCH_WIDGET,
                    IntentOrigin.QUICK_ACTION_SEARCH_WIDGET,
                    IntentOrigin.CUSTOM_TAB,
                };

        SearchActivityClient client = new SearchActivityUtils();
        for (int origin : origins) {
            // null URL
            var intent = client.createIntent(mActivity, origin, null, SearchType.VOICE);
            assertEquals(SearchActivityUtils.ACTION_VOICE_SEARCH, intent.getAction());
            assertNull(intent.getStringExtra(SearchActivityUtils.EXTRA_CURRENT_URL));
            assertEquals(SearchType.VOICE, SearchActivityUtils.getIntentSearchType(intent));
            assertEquals(origin, SearchActivityUtils.getIntentOrigin(intent));

            // non-null URL
            intent =
                    client.createIntent(
                            mActivity, origin, new GURL("http://abc.xyz"), SearchType.VOICE);
            assertEquals(SearchActivityUtils.ACTION_VOICE_SEARCH, intent.getAction());
            assertEquals(
                    "http://abc.xyz/",
                    intent.getStringExtra(SearchActivityUtils.EXTRA_CURRENT_URL));
            assertEquals(SearchType.VOICE, SearchActivityUtils.getIntentSearchType(intent));
            assertEquals(origin, SearchActivityUtils.getIntentOrigin(intent));
        }
    }

    @Test
    public void createIntent_forLensSearch() {
        @IntentOrigin
        int[] origins =
                new int[] {
                    IntentOrigin.SEARCH_WIDGET,
                    IntentOrigin.QUICK_ACTION_SEARCH_WIDGET,
                    IntentOrigin.CUSTOM_TAB,
                };

        SearchActivityClient client = new SearchActivityUtils();
        for (int origin : origins) {
            // null URL
            var intent = client.createIntent(mActivity, origin, null, SearchType.LENS);
            assertEquals(SearchActivityUtils.ACTION_LENS_SEARCH, intent.getAction());
            assertNull(intent.getStringExtra(SearchActivityUtils.EXTRA_CURRENT_URL));
            assertEquals(SearchType.LENS, SearchActivityUtils.getIntentSearchType(intent));
            assertEquals(origin, SearchActivityUtils.getIntentOrigin(intent));

            // non-null URL
            intent =
                    client.createIntent(
                            mActivity, origin, new GURL("http://abc.xyz"), SearchType.LENS);
            assertEquals(SearchActivityUtils.ACTION_LENS_SEARCH, intent.getAction());
            assertEquals(
                    "http://abc.xyz/",
                    intent.getStringExtra(SearchActivityUtils.EXTRA_CURRENT_URL));
            assertEquals(SearchType.LENS, SearchActivityUtils.getIntentSearchType(intent));
            assertEquals(origin, SearchActivityUtils.getIntentOrigin(intent));
        }
    }

    @Test
    public void buildTrustedIntent_appliesExpectedAction() {
        var intent = SearchActivityUtils.buildTrustedIntent(mActivity, "abcd");
        assertEquals("abcd", intent.getAction());

        intent = SearchActivityUtils.buildTrustedIntent(mActivity, "1234");
        assertEquals("1234", intent.getAction());
    }

    @Test
    public void buildTrustedIntent_addressesSearchActivity() {
        var intent = SearchActivityUtils.buildTrustedIntent(mActivity, "a");
        assertEquals(
                intent.getComponent().getClassName().toString(), SearchActivity.class.getName());
    }

    @Test
    public void buildTrustedIntent_intentIsTrusted() {
        var intent = SearchActivityUtils.buildTrustedIntent(mActivity, "a");
        assertTrue(IntentUtils.isTrustedIntentFromSelf(intent));
    }

    @Test
    public void requestOmniboxForResult_noActionWhenActivityIsNull() {
        SearchActivityUtils.requestOmniboxForResult(null, EMPTY_URL);
    }

    @Test
    public void requestOmniboxForResult_propagatesCurrentUrl() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, GOOD_URL);

        var intentForResult = Shadows.shadowOf(mActivity).getNextStartedActivityForResult();

        assertEquals(
                IntentUtils.safeGetStringExtra(
                        intentForResult.intent, SearchActivityUtils.EXTRA_CURRENT_URL),
                GOOD_URL.getSpec());
        assertEquals(SearchActivityUtils.OMNIBOX_REQUEST_CODE, intentForResult.requestCode);
    }

    @Test
    public void requestOmniboxForResult_acceptsEmptyUrl() {
        // This is technically an invalid case. The test verifies we still do the right thing.
        SearchActivityUtils.requestOmniboxForResult(mActivity, EMPTY_URL);

        var intentForResult = Shadows.shadowOf(mActivity).getNextStartedActivityForResult();

        assertTrue(
                IntentUtils.safeHasExtra(
                        intentForResult.intent, SearchActivityUtils.EXTRA_CURRENT_URL));
        assertTrue(
                TextUtils.isEmpty(
                        IntentUtils.safeGetStringExtra(
                                intentForResult.intent, SearchActivityUtils.EXTRA_CURRENT_URL)));
        assertEquals(SearchActivityUtils.OMNIBOX_REQUEST_CODE, intentForResult.requestCode);
    }

    @Test
    public void getIntentOrigin_forOmniboxRequestForResult() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, EMPTY_URL);

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertEquals(IntentOrigin.CUSTOM_TAB, SearchActivityUtils.getIntentOrigin(intent));
    }

    @Test
    public void getIntentOrigin_forSearchWidgetRequest() {
        Intent intent = buildSearchWidgetIntent();
        assertEquals(IntentOrigin.SEARCH_WIDGET, SearchActivityUtils.getIntentOrigin(intent));
    }

    @Test
    public void getIntentOrigin_untrustedIntent() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, EMPTY_URL);

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertEquals(IntentOrigin.UNKNOWN, SearchActivityUtils.getIntentOrigin(intent));
    }

    @Test
    public void getOmniboxRequestType_omniboxRequestForResultMissingData() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, EMPTY_URL);

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        intent.removeExtra(SearchActivityUtils.EXTRA_CURRENT_URL);
        assertEquals(IntentOrigin.UNKNOWN, SearchActivityUtils.getIntentOrigin(intent));
    }

    @Test
    public void getIntentSearchType_forCustomTab() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, EMPTY_URL);

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertEquals(IntentOrigin.CUSTOM_TAB, SearchActivityUtils.getIntentOrigin(intent));

        // Invalid variants
        intent.setAction(SearchActivityConstants.ACTION_START_TEXT_SEARCH);
        assertEquals(SearchType.TEXT, SearchActivityUtils.getIntentSearchType(intent));

        intent.setAction(SearchActivityConstants.ACTION_START_VOICE_SEARCH);
        assertEquals(SearchType.TEXT, SearchActivityUtils.getIntentSearchType(intent));

        intent.setAction(null);
        assertEquals(SearchType.TEXT, SearchActivityUtils.getIntentSearchType(intent));

        intent.setAction("abcd");
        assertEquals(SearchType.TEXT, SearchActivityUtils.getIntentSearchType(intent));
    }

    @Test
    public void getIntentSearchType_forSearchWidget() {
        var intent = buildSearchWidgetIntent();
        assertEquals(IntentOrigin.SEARCH_WIDGET, SearchActivityUtils.getIntentOrigin(intent));

        intent.setAction(SearchActivityConstants.ACTION_START_TEXT_SEARCH);
        assertEquals(SearchType.TEXT, SearchActivityUtils.getIntentSearchType(intent));

        intent.setAction(SearchActivityConstants.ACTION_START_VOICE_SEARCH);
        assertEquals(SearchType.VOICE, SearchActivityUtils.getIntentSearchType(intent));

        // Invalid variants
        intent.setAction(null);
        assertEquals(SearchType.TEXT, SearchActivityUtils.getIntentSearchType(intent));

        intent.setAction("abcd");
        assertEquals(SearchType.TEXT, SearchActivityUtils.getIntentSearchType(intent));
    }

    @Test
    public void resolveOmniboxRequestForResult_successfulResolutionForValidGURL() {
        // Simulate environment where we received an intent from self.
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingActivity(
                new ComponentName(ContextUtils.getApplicationContext(), TestActivity.class));

        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, GOOD_URL);

        var intent = Shadows.shadowOf(mActivity).getResultIntent();
        assertTrue(intent.hasExtra(SearchActivityUtils.EXTRA_URL_TO_NAVIGATE));
        assertEquals(
                GOOD_URL.getSpec(),
                IntentUtils.safeGetStringExtra(intent, SearchActivityUtils.EXTRA_URL_TO_NAVIGATE));
        assertEquals(Activity.RESULT_OK, Shadows.shadowOf(mActivity).getResultCode());
    }

    @Test
    public void resolveOmniboxRequestForResult_noTrustedExtrasWithUnexpectedCallingPackage() {
        // An unlikely scenario where the caller somehow managed to pass a valid trusted token.
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingPackage("com.abc.xyz");

        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, GOOD_URL);

        var intent = Shadows.shadowOf(mActivity).getResultIntent();
        assertFalse(intent.hasExtra(SearchActivityUtils.EXTRA_URL_TO_NAVIGATE));
        assertFalse(IntentUtils.safeHasExtra(intent, SearchActivityUtils.EXTRA_URL_TO_NAVIGATE));

        // Respectfully tell the caller we have nothing else to share.
        assertEquals(Activity.RESULT_OK, Shadows.shadowOf(mActivity).getResultCode());
    }

    @Test
    public void resolveOmniboxRequestForResult_canceledResolutionForNullOrInvalidGURLs() {
        var activity = Shadows.shadowOf(mActivity);

        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, null);
        assertEquals(Activity.RESULT_CANCELED, activity.getResultCode());

        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, GURL.emptyGURL());
        assertEquals(Activity.RESULT_CANCELED, activity.getResultCode());

        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, new GURL("a b"));
        assertEquals(Activity.RESULT_CANCELED, activity.getResultCode());
    }

    @Test
    public void isOmniboxResult_validResponse() {
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingActivity(
                new ComponentName(ContextUtils.getApplicationContext(), TestActivity.class));
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, GOOD_URL);
        var intent = Shadows.shadowOf(mActivity).getResultIntent();

        // Our own responses should always be valid.
        assertTrue(
                SearchActivityUtils.isOmniboxResult(
                        SearchActivityUtils.OMNIBOX_REQUEST_CODE, intent));
    }

    @Test
    public void isOmniboxResult_invalidRequestCode() {
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingActivity(
                new ComponentName(ContextUtils.getApplicationContext(), TestActivity.class));
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, GOOD_URL);
        var intent = Shadows.shadowOf(mActivity).getResultIntent();

        assertFalse(
                SearchActivityUtils.isOmniboxResult(
                        SearchActivityUtils.OMNIBOX_REQUEST_CODE - 1, intent));
        assertFalse(SearchActivityUtils.isOmniboxResult(0, intent));
        assertFalse(SearchActivityUtils.isOmniboxResult(~0, intent));
    }

    @Test
    public void isOmniboxResult_untrustedReply() {
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingActivity(
                new ComponentName(ContextUtils.getApplicationContext(), TestActivity.class));
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, GOOD_URL);
        var intent = Shadows.shadowOf(mActivity).getResultIntent();

        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertFalse(
                SearchActivityUtils.isOmniboxResult(
                        SearchActivityUtils.OMNIBOX_REQUEST_CODE, intent));
    }

    @Test
    public void isOmniboxResult_missingDestinationUrl() {
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingActivity(
                new ComponentName(ContextUtils.getApplicationContext(), TestActivity.class));
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, GOOD_URL);
        var intent = Shadows.shadowOf(mActivity).getResultIntent();

        intent.removeExtra(SearchActivityUtils.EXTRA_URL_TO_NAVIGATE);
        assertFalse(
                SearchActivityUtils.isOmniboxResult(
                        SearchActivityUtils.OMNIBOX_REQUEST_CODE, intent));
    }

    @Test
    public void getOmniboxResult_successfulResolution() {
        // Resolve intent with GOOD_URL.
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingActivity(
                new ComponentName(ContextUtils.getApplicationContext(), TestActivity.class));
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, GOOD_URL);

        // We should see the same URL on the receiving side.
        var intent = Shadows.shadowOf(mActivity).getResultIntent();
        assertEquals(
                GOOD_URL.getSpec(),
                SearchActivityUtils.getOmniboxResult(
                                SearchActivityUtils.OMNIBOX_REQUEST_CODE,
                                Activity.RESULT_OK,
                                intent)
                        .getSpec());
    }

    @Test
    public void getOmniboxResult_returnsNullForNonOmniboxResult() {
        // Resolve intent with GOOD_URL. Note, we don't want to get caught in early returns - make
        // sure our intent is valid.
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingActivity(
                new ComponentName(ContextUtils.getApplicationContext(), TestActivity.class));
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, GOOD_URL);

        // We should see no GURL object on the receiving side: this is not our intent.
        var intent = Shadows.shadowOf(mActivity).getResultIntent();
        assertNull(
                SearchActivityUtils.getOmniboxResult(
                        /* requestCode= */ ~0, Activity.RESULT_OK, intent));
    }

    @Test
    public void getOmniboxResult_returnsEmptyGURLForCanceledNavigation() {
        // Resolve intent with GOOD_URL. Note, we don't want to get caught in early returns - make
        // sure our intent is valid.
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingActivity(
                new ComponentName(ContextUtils.getApplicationContext(), TestActivity.class));
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, GOOD_URL);

        // We should see an empty GURL on the receiving side.
        var intent = Shadows.shadowOf(mActivity).getResultIntent();
        assertTrue(
                SearchActivityUtils.getOmniboxResult(
                                SearchActivityUtils.OMNIBOX_REQUEST_CODE,
                                Activity.RESULT_CANCELED,
                                intent)
                        .isEmpty());
    }
}
