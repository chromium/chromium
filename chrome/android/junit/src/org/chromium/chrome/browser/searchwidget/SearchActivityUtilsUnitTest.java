// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.app.SearchManager;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient.SearchType;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ResourceRequestBodyJni;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {SearchActivityUtilsUnitTest.ShadowUrlFormatter.class})
public class SearchActivityUtilsUnitTest {
    // Placeholder Activity class that guarantees the PackageName is valid for IntentUtils.
    private static class TestActivity extends Activity {}

    private static final GURL GOOD_URL = new GURL("https://abc.xyz");
    private static final GURL EMPTY_URL = GURL.emptyGURL();
    private static final OmniboxLoadUrlParams LOAD_URL_PARAMS_NULL_URL =
            new OmniboxLoadUrlParams.Builder(null, PageTransition.TYPED).build();
    private static final OmniboxLoadUrlParams LOAD_URL_PARAMS_INVALID_URL =
            new OmniboxLoadUrlParams.Builder("abcde", PageTransition.TYPED).build();
    private static final ComponentName COMPONENT_TRUSTED =
            new ComponentName(ContextUtils.getApplicationContext(), SearchActivity.class);
    private static final ComponentName COMPONENT_UNTRUSTED =
            new ComponentName("com.some.package", "com.some.package.test.Activity");

    private Activity mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();

    // UrlFormatter call intercepting mock.
    private interface TestUrlFormatter {
        GURL fixupUrl(String uri);
    }

    @Implements(UrlFormatter.class)
    public static class ShadowUrlFormatter {
        static TestUrlFormatter sMockFormatter;

        @Implementation
        public static GURL fixupUrl(String uri) {
            return sMockFormatter.fixupUrl(uri);
        }
    }

    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    public @Rule JniMocker mJniMocker = new JniMocker();
    private @Mock TestUrlFormatter mFormatter;
    private @Mock ResourceRequestBodyJni mResourceRequestBodyJni;

    @Before
    public void setUp() {
        ShadowUrlFormatter.sMockFormatter = mFormatter;
        mJniMocker.mock(ResourceRequestBodyJni.TEST_HOOKS, mResourceRequestBodyJni);
        doAnswer(i -> i.getArgument(0))
                .when(mResourceRequestBodyJni)
                .createResourceRequestBodyFromBytes(any());

        doAnswer(
                        i -> {
                            return new GURL(i.getArgument(0));
                        })
                .when(mFormatter)
                .fixupUrl(any());
    }

    private OmniboxLoadUrlParams.Builder getLoadUrlParamsBuilder() {
        return new OmniboxLoadUrlParams.Builder("https://abc.xyz", PageTransition.TYPED);
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
            String action =
                    String.format(
                            SearchActivityUtils.ACTION_SEARCH_FORMAT, origin, SearchType.TEXT);

            // null URL
            var intent = client.createIntent(mActivity, origin, null, SearchType.TEXT);
            assertEquals(action, intent.getAction());
            assertNull(intent.getStringExtra(SearchActivityUtils.EXTRA_CURRENT_URL));
            assertEquals(SearchType.TEXT, SearchActivityUtils.getIntentSearchType(intent));
            assertEquals(origin, SearchActivityUtils.getIntentOrigin(intent));

            // non-null URL
            intent =
                    client.createIntent(
                            mActivity, origin, new GURL("http://abc.xyz"), SearchType.TEXT);
            assertEquals(action, intent.getAction());
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
            String action =
                    String.format(
                            SearchActivityUtils.ACTION_SEARCH_FORMAT, origin, SearchType.VOICE);

            // null URL
            var intent = client.createIntent(mActivity, origin, null, SearchType.VOICE);
            assertEquals(action, intent.getAction());
            assertNull(intent.getStringExtra(SearchActivityUtils.EXTRA_CURRENT_URL));
            assertEquals(SearchType.VOICE, SearchActivityUtils.getIntentSearchType(intent));
            assertEquals(origin, SearchActivityUtils.getIntentOrigin(intent));

            // non-null URL
            intent =
                    client.createIntent(
                            mActivity, origin, new GURL("http://abc.xyz"), SearchType.VOICE);
            assertEquals(action, intent.getAction());
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
            String action =
                    String.format(
                            SearchActivityUtils.ACTION_SEARCH_FORMAT, origin, SearchType.LENS);

            // null URL
            var intent = client.createIntent(mActivity, origin, null, SearchType.LENS);
            assertEquals(action, intent.getAction());
            assertNull(intent.getStringExtra(SearchActivityUtils.EXTRA_CURRENT_URL));
            assertEquals(SearchType.LENS, SearchActivityUtils.getIntentSearchType(intent));
            assertEquals(origin, SearchActivityUtils.getIntentOrigin(intent));

            // non-null URL
            intent =
                    client.createIntent(
                            mActivity, origin, new GURL("http://abc.xyz"), SearchType.LENS);
            assertEquals(action, intent.getAction());
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
        SearchActivityUtils.requestOmniboxForResult(null, EMPTY_URL, null);
    }

    @Test
    public void requestOmniboxForResult_propagatesCurrentUrl() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, GOOD_URL, null);

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
        SearchActivityUtils.requestOmniboxForResult(mActivity, EMPTY_URL, null);

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
    public void getIntentOrigin_trustedIntent() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, EMPTY_URL, null);

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertEquals(IntentOrigin.CUSTOM_TAB, SearchActivityUtils.getIntentOrigin(intent));
    }

    @Test
    public void getIntentOrigin_untrustedIntent() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, EMPTY_URL, null);

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertEquals(IntentOrigin.UNKNOWN, SearchActivityUtils.getIntentOrigin(intent));
    }

    @Test
    public void getIntentSearchType_trustedIntent() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, EMPTY_URL, null);

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertEquals(IntentOrigin.CUSTOM_TAB, SearchActivityUtils.getIntentOrigin(intent));

        // Invalid variants
        intent.setAction(null);
        assertEquals(SearchType.TEXT, SearchActivityUtils.getIntentSearchType(intent));

        intent.setAction("abcd");
        assertEquals(SearchType.TEXT, SearchActivityUtils.getIntentSearchType(intent));
    }

    @Test
    public void getIntentSearchType_untrustedIntent() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, EMPTY_URL, null);

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertEquals(SearchType.TEXT, SearchActivityUtils.getIntentSearchType(intent));
    }

    @Test
    public void getIntentUrl_forNullUrl() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, null, null);
        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertNull(SearchActivityUtils.getIntentUrl(intent));
        // Remove trust
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertNull(SearchActivityUtils.getIntentUrl(intent));
    }

    @Test
    public void getIntentUrl_forEmptyUrl() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, GURL.emptyGURL(), null);
        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertNull(SearchActivityUtils.getIntentUrl(intent));
        // Remove trust
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertNull(SearchActivityUtils.getIntentUrl(intent));
    }

    @Test
    public void getIntentUrl_forInvalidUrl() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, new GURL("abcd"), null);
        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertNull(SearchActivityUtils.getIntentUrl(intent));
        // Remove trust
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertNull(SearchActivityUtils.getIntentUrl(intent));
    }

    @Test
    public void getIntentUrl_forValidUrl() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, new GURL("https://abc.xyz"), null);
        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertEquals("https://abc.xyz/", SearchActivityUtils.getIntentUrl(intent).getSpec());
        // Remove trust
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertNull(SearchActivityUtils.getIntentUrl(intent));
    }

    @Test
    public void getIntentSearchType_emptyPackageName() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, GOOD_URL, "");

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertEquals(IntentOrigin.CUSTOM_TAB, SearchActivityUtils.getIntentOrigin(intent));
        assertNull(SearchActivityUtils.getReferrer(intent));

        // Remove trust
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertNull(SearchActivityUtils.getReferrer(intent));
    }

    @Test
    public void getIntentSearchType_nullPackageName() {
        SearchActivityUtils.requestOmniboxForResult(mActivity, GOOD_URL, null);

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertEquals(IntentOrigin.CUSTOM_TAB, SearchActivityUtils.getIntentOrigin(intent));
        assertNull(SearchActivityUtils.getReferrer(intent));

        // Remove trust
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertNull(SearchActivityUtils.getReferrer(intent));
    }

    @Test
    public void getIntentSearchType_validPackageName() {
        var cases = List.of("ab", "a.b", "a-b", "0.9", "a.0", "k-9", "A_Z", "ABC123");

        for (var testCase : cases) {
            SearchActivityUtils.requestOmniboxForResult(mActivity, GOOD_URL, testCase);
            var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
            assertEquals(testCase, SearchActivityUtils.getReferrer(intent));
            // Remove trust
            intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
            assertNull(SearchActivityUtils.getReferrer(intent));
        }
    }

    @Test
    public void getIntentSearchType_invalidPackageName() {
        var cases = List.of("a", "a.", ".a", "a&b", "a?b", "a+b", "a$b", "a_");

        for (var testCase : cases) {
            SearchActivityUtils.requestOmniboxForResult(mActivity, GOOD_URL, testCase);
            var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
            assertNull(SearchActivityUtils.getReferrer(intent));
        }
    }

    @Test
    public void resolveOmniboxRequestForResult_successfulResolutionForValidGURL() {
        // Simulate environment where we received an intent from self.
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingActivity(
                new ComponentName(ContextUtils.getApplicationContext(), TestActivity.class));

        var params = getLoadUrlParamsBuilder().build();
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, params);

        var intent = Shadows.shadowOf(mActivity).getResultIntent();
        assertEquals("https://abc.xyz/", intent.getDataString());
        assertEquals(Activity.RESULT_OK, Shadows.shadowOf(mActivity).getResultCode());
    }

    @Test
    public void resolveOmniboxRequestForResult_noTrustedExtrasWithUnexpectedCallingPackage() {
        // An unlikely scenario where the caller somehow managed to pass a valid trusted token.
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingPackage("com.abc.xyz");

        var params = getLoadUrlParamsBuilder().build();
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, params);

        var intent = Shadows.shadowOf(mActivity).getResultIntent();
        assertNull(intent);

        // Respectfully tell the caller we have nothing else to share.
        assertEquals(Activity.RESULT_CANCELED, Shadows.shadowOf(mActivity).getResultCode());
    }

    @Test
    public void resolveOmniboxRequestForResult_canceledResolutionForNullOrInvalidGURLs() {
        var activity = Shadows.shadowOf(mActivity);

        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, null);
        assertEquals(Activity.RESULT_CANCELED, activity.getResultCode());

        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, LOAD_URL_PARAMS_NULL_URL);
        assertEquals(Activity.RESULT_CANCELED, activity.getResultCode());

        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, LOAD_URL_PARAMS_INVALID_URL);
        assertEquals(Activity.RESULT_CANCELED, activity.getResultCode());
    }

    @Test
    public void isOmniboxResult_validResponse() {
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingActivity(
                new ComponentName(ContextUtils.getApplicationContext(), TestActivity.class));
        var params = getLoadUrlParamsBuilder().build();
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, params);
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
        var params = getLoadUrlParamsBuilder().build();
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, params);
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
        var params = getLoadUrlParamsBuilder().build();
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, params);
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
        var params = getLoadUrlParamsBuilder().build();
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, params);
        var intent = Shadows.shadowOf(mActivity).getResultIntent();

        intent.setData(null);
        assertFalse(
                SearchActivityUtils.isOmniboxResult(
                        SearchActivityUtils.OMNIBOX_REQUEST_CODE, intent));
    }

    @Test
    public void getIntentQuery_noQuery() {
        var intent = new Intent();
        assertNull(SearchActivityUtils.getIntentQuery(intent));
    }

    @Test
    public void getIntentQuery_nullQuery() {
        var intent = new Intent();
        intent.putExtra(SearchManager.QUERY, (String) null);
        assertNull(SearchActivityUtils.getIntentQuery(intent));
    }

    @Test
    public void getIntentQuery_invalidQuery() {
        var intent = new Intent();
        intent.putExtra(SearchManager.QUERY, true);
        assertNull(SearchActivityUtils.getIntentQuery(intent));
    }

    @Test
    public void getIntentQuery_emptyQuery() {
        var intent = new Intent();
        intent.putExtra(SearchManager.QUERY, "");
        assertEquals("", SearchActivityUtils.getIntentQuery(intent));
    }

    @Test
    public void getIntentQuery_withQuery() {
        var intent = new Intent();
        intent.putExtra(SearchManager.QUERY, "query");
        assertEquals("query", SearchActivityUtils.getIntentQuery(intent));
    }

    @Test
    public void getOmniboxResult_withInvalidUrl() {
        var intent = new Intent();
        intent.setComponent(COMPONENT_TRUSTED);
        intent.setData(Uri.parse("a b"));
        IntentUtils.addTrustedIntentExtras(intent);
        assertNull(
                SearchActivityUtils.getOmniboxResult(
                        SearchActivityUtils.OMNIBOX_REQUEST_CODE, Activity.RESULT_OK, intent));
    }

    @Test
    public void getOmniboxResult_successfulResolution_simple() {
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingActivity(
                new ComponentName(ContextUtils.getApplicationContext(), TestActivity.class));
        var params = getLoadUrlParamsBuilder().build();
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, params);

        var intent = Shadows.shadowOf(mActivity).getResultIntent();
        LoadUrlParams result =
                SearchActivityUtils.getOmniboxResult(
                        SearchActivityUtils.OMNIBOX_REQUEST_CODE, Activity.RESULT_OK, intent);

        assertEquals("https://abc.xyz/", result.getUrl());
        assertNull(result.getVerbatimHeaders());
        assertNull(result.getPostData());
    }

    @Test
    public void getOmniboxResult_successfulResolution_withPostDataOnly() {
        var intent = new Intent();
        intent.setComponent(COMPONENT_TRUSTED);
        intent.setData(Uri.parse("https://abc.xyz"));
        intent.putExtra(IntentHandler.EXTRA_POST_DATA, new byte[] {1, 2});
        IntentUtils.addTrustedIntentExtras(intent);

        LoadUrlParams result =
                SearchActivityUtils.getOmniboxResult(
                        SearchActivityUtils.OMNIBOX_REQUEST_CODE, Activity.RESULT_OK, intent);

        assertEquals("https://abc.xyz/", result.getUrl());
        assertNull(result.getVerbatimHeaders());
        assertNull(result.getPostData());
    }

    @Test
    public void getOmniboxResult_successfulResolution_withPostDataTypeOnly() {
        var intent = new Intent();
        intent.setComponent(COMPONENT_TRUSTED);
        intent.setData(Uri.parse("https://abc.xyz"));
        intent.putExtra(IntentHandler.EXTRA_POST_DATA_TYPE, "data");
        IntentUtils.addTrustedIntentExtras(intent);

        LoadUrlParams result =
                SearchActivityUtils.getOmniboxResult(
                        SearchActivityUtils.OMNIBOX_REQUEST_CODE, Activity.RESULT_OK, intent);

        assertEquals("https://abc.xyz/", result.getUrl());
        assertNull(result.getVerbatimHeaders());
        assertNull(result.getPostData());
    }

    @Test
    public void getOmniboxResult_successfulResolution_withEmptyPostData() {
        var intent = new Intent();
        intent.setComponent(COMPONENT_TRUSTED);
        intent.setData(Uri.parse("https://abc.xyz"));
        intent.putExtra(IntentHandler.EXTRA_POST_DATA_TYPE, "data");
        intent.putExtra(IntentHandler.EXTRA_POST_DATA, new byte[] {});
        IntentUtils.addTrustedIntentExtras(intent);

        LoadUrlParams result =
                SearchActivityUtils.getOmniboxResult(
                        SearchActivityUtils.OMNIBOX_REQUEST_CODE, Activity.RESULT_OK, intent);

        assertEquals("https://abc.xyz/", result.getUrl());
        assertNull(result.getVerbatimHeaders());
        assertNull(result.getPostData());
    }

    @Test
    public void getOmniboxResult_successfulResolution_withCompletePostData() {
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingActivity(
                new ComponentName(ContextUtils.getApplicationContext(), TestActivity.class));
        var params =
                getLoadUrlParamsBuilder().setpostDataAndType(new byte[] {1, 2}, "data").build();
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, params);

        // We should see the same URL on the receiving side.
        var intent = Shadows.shadowOf(mActivity).getResultIntent();
        LoadUrlParams result =
                SearchActivityUtils.getOmniboxResult(
                        SearchActivityUtils.OMNIBOX_REQUEST_CODE, Activity.RESULT_OK, intent);

        assertEquals("https://abc.xyz/", result.getUrl());
        assertEquals("Content-Type: data", result.getVerbatimHeaders());
        assertArrayEquals(new byte[] {1, 2}, result.getPostData().getEncodedNativeForm());
    }

    @Test
    public void getOmniboxResult_returnsNullForNonOmniboxResult() {
        // Resolve intent with GOOD_URL. Note, we don't want to get caught in early returns - make
        // sure our intent is valid.
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingActivity(
                new ComponentName(ContextUtils.getApplicationContext(), TestActivity.class));
        var params = getLoadUrlParamsBuilder().build();
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, params);

        // We should see no GURL object on the receiving side: this is not our intent.
        var intent = Shadows.shadowOf(mActivity).getResultIntent();
        assertNull(
                SearchActivityUtils.getOmniboxResult(
                        /* requestCode= */ ~0, Activity.RESULT_OK, intent));
    }

    @Test
    public void getOmniboxResult_returnsNullForCanceledNavigation() {
        // Resolve intent with GOOD_URL. Note, we don't want to get caught in early returns - make
        // sure our intent is valid.
        var activity = Shadows.shadowOf(mActivity);
        activity.setCallingActivity(
                new ComponentName(ContextUtils.getApplicationContext(), TestActivity.class));
        var params = getLoadUrlParamsBuilder().build();
        SearchActivityUtils.resolveOmniboxRequestForResult(mActivity, params);

        // We should see an empty GURL on the receiving side.
        var intent = Shadows.shadowOf(mActivity).getResultIntent();
        assertNull(
                SearchActivityUtils.getOmniboxResult(
                        SearchActivityUtils.OMNIBOX_REQUEST_CODE,
                        Activity.RESULT_CANCELED,
                        intent));
    }

    @Test
    public void createLoadUrlIntent_nullParams() {
        assertNull(SearchActivityUtils.createLoadUrlIntent(mActivity, COMPONENT_TRUSTED, null));
    }

    @Test
    public void createLoadUrlIntent_nullUrl() {
        assertNull(
                SearchActivityUtils.createLoadUrlIntent(
                        mActivity, COMPONENT_TRUSTED, LOAD_URL_PARAMS_NULL_URL));
    }

    @Test
    public void createLoadUrlIntent_invalidUrl() {
        assertNull(
                SearchActivityUtils.createLoadUrlIntent(
                        mActivity, COMPONENT_TRUSTED, LOAD_URL_PARAMS_INVALID_URL));
    }

    @Test
    public void createLoadUrlIntent_invalidFixedUpUrl() {
        doReturn(null).when(mFormatter).fixupUrl(any());
        assertNull(
                SearchActivityUtils.createLoadUrlIntent(
                        mActivity, COMPONENT_TRUSTED, getLoadUrlParamsBuilder().build()));
    }

    @Test
    public void createLoadUrlIntent_untrustedRecipient() {
        Intent intent =
                SearchActivityUtils.createLoadUrlIntent(
                        mActivity, COMPONENT_UNTRUSTED, getLoadUrlParamsBuilder().build());
        assertNull(intent);
    }

    @Test
    public void createLoadUrlIntent_simpleParams() {
        Intent intent =
                SearchActivityUtils.createLoadUrlIntent(
                        mActivity, COMPONENT_TRUSTED, getLoadUrlParamsBuilder().build());
        assertNotNull(intent);

        assertEquals(Uri.parse("https://abc.xyz/"), intent.getData());
        assertTrue(intent.getBooleanExtra(SearchActivity.EXTRA_FROM_SEARCH_ACTIVITY, false));
        assertEquals(COMPONENT_TRUSTED.getClassName(), intent.getComponent().getClassName());
        assertNull(intent.getStringExtra(IntentHandler.EXTRA_POST_DATA_TYPE));
        assertNull(intent.getByteArrayExtra(IntentHandler.EXTRA_POST_DATA));
        assertTrue(intent.hasExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA));
    }

    @Test
    public void createLoadUrlIntent_paramsWithNullPostData() {
        var params = getLoadUrlParamsBuilder().setpostDataAndType(null, "abc").build();
        Intent intent =
                SearchActivityUtils.createLoadUrlIntent(mActivity, COMPONENT_TRUSTED, params);
        assertNotNull(intent);

        assertEquals(Uri.parse("https://abc.xyz/"), intent.getData());
        assertTrue(intent.getBooleanExtra(SearchActivity.EXTRA_FROM_SEARCH_ACTIVITY, false));
        assertEquals(COMPONENT_TRUSTED.getClassName(), intent.getComponent().getClassName());
        assertNull(intent.getStringExtra(IntentHandler.EXTRA_POST_DATA_TYPE));
        assertNull(intent.getByteArrayExtra(IntentHandler.EXTRA_POST_DATA));
        assertTrue(intent.hasExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA));
    }

    @Test
    public void createLoadUrlIntent_paramsWithEmptyPostData() {
        var params = getLoadUrlParamsBuilder().setpostDataAndType(new byte[] {}, "abc").build();
        Intent intent =
                SearchActivityUtils.createLoadUrlIntent(mActivity, COMPONENT_TRUSTED, params);
        assertNotNull(intent);

        assertEquals(Uri.parse("https://abc.xyz/"), intent.getData());
        assertTrue(intent.getBooleanExtra(SearchActivity.EXTRA_FROM_SEARCH_ACTIVITY, false));
        assertEquals(COMPONENT_TRUSTED.getClassName(), intent.getComponent().getClassName());
        assertNull(intent.getStringExtra(IntentHandler.EXTRA_POST_DATA_TYPE));
        assertNull(intent.getByteArrayExtra(IntentHandler.EXTRA_POST_DATA));
        assertTrue(intent.hasExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA));
    }

    @Test
    public void createLoadUrlIntent_paramsWithNullPostDataType() {
        var params =
                getLoadUrlParamsBuilder().setpostDataAndType(new byte[] {1, 2, 3}, null).build();
        Intent intent =
                SearchActivityUtils.createLoadUrlIntent(mActivity, COMPONENT_TRUSTED, params);
        assertNotNull(intent);

        assertEquals(Uri.parse("https://abc.xyz/"), intent.getData());
        assertTrue(intent.getBooleanExtra(SearchActivity.EXTRA_FROM_SEARCH_ACTIVITY, false));
        assertEquals(COMPONENT_TRUSTED.getClassName(), intent.getComponent().getClassName());
        assertNull(intent.getStringExtra(IntentHandler.EXTRA_POST_DATA_TYPE));
        assertNull(intent.getByteArrayExtra(IntentHandler.EXTRA_POST_DATA));
        assertTrue(intent.hasExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA));
    }

    @Test
    public void createLoadUrlIntent_paramsWithEmptyPostDataType() {
        var params = getLoadUrlParamsBuilder().setpostDataAndType(new byte[] {1, 2, 3}, "").build();
        Intent intent =
                SearchActivityUtils.createLoadUrlIntent(mActivity, COMPONENT_TRUSTED, params);
        assertNotNull(intent);

        assertEquals(Uri.parse("https://abc.xyz/"), intent.getData());
        assertTrue(intent.getBooleanExtra(SearchActivity.EXTRA_FROM_SEARCH_ACTIVITY, false));
        assertEquals(COMPONENT_TRUSTED.getClassName(), intent.getComponent().getClassName());
        assertNull(intent.getStringExtra(IntentHandler.EXTRA_POST_DATA_TYPE));
        assertNull(intent.getByteArrayExtra(IntentHandler.EXTRA_POST_DATA));
        assertTrue(intent.hasExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA));
    }

    @Test
    public void createLoadUrlIntent_paramsWithValidPostDataType() {
        var params =
                getLoadUrlParamsBuilder().setpostDataAndType(new byte[] {1, 2, 3}, "test").build();
        Intent intent =
                SearchActivityUtils.createLoadUrlIntent(mActivity, COMPONENT_TRUSTED, params);
        assertNotNull(intent);

        assertEquals(Uri.parse("https://abc.xyz/"), intent.getData());
        assertTrue(intent.getBooleanExtra(SearchActivity.EXTRA_FROM_SEARCH_ACTIVITY, false));
        assertEquals(COMPONENT_TRUSTED.getClassName(), intent.getComponent().getClassName());
        assertEquals("test", intent.getStringExtra(IntentHandler.EXTRA_POST_DATA_TYPE));
        assertArrayEquals(
                new byte[] {1, 2, 3}, intent.getByteArrayExtra(IntentHandler.EXTRA_POST_DATA));
        assertTrue(intent.hasExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA));
    }
}
