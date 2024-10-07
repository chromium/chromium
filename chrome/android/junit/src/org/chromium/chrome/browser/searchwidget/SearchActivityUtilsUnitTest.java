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
import static org.mockito.Mockito.spy;

import android.app.Activity;
import android.app.SearchManager;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;

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
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.components.url_formatter.UrlFormatter;
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

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
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

        doAnswer(i -> new GURL(i.getArgument(0))).when(mFormatter).fixupUrl(any());
    }

    private OmniboxLoadUrlParams.Builder getLoadUrlParamsBuilder() {
        return new OmniboxLoadUrlParams.Builder("https://abc.xyz", PageTransition.TYPED);
    }

    @Test
    public void getIntentOrigin_trustedIntent() {
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        mActivity,
                        EMPTY_URL,
                        IntentOrigin.CUSTOM_TAB,
                        null,
                        /* isIncognito= */ false);

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertEquals(IntentOrigin.CUSTOM_TAB, SearchActivityUtils.getIntentOrigin(intent));
    }

    @Test
    public void getIntentOrigin_untrustedIntent() {
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        mActivity,
                        EMPTY_URL,
                        IntentOrigin.CUSTOM_TAB,
                        null,
                        /* isIncognito= */ false);

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertEquals(IntentOrigin.UNKNOWN, SearchActivityUtils.getIntentOrigin(intent));
    }

    @Test
    public void getIntentSearchType_trustedIntent() {
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        mActivity,
                        EMPTY_URL,
                        IntentOrigin.CUSTOM_TAB,
                        null,
                        /* isIncognito= */ false);

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
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        mActivity,
                        EMPTY_URL,
                        IntentOrigin.CUSTOM_TAB,
                        null,
                        /* isIncognito= */ false);

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertEquals(SearchType.TEXT, SearchActivityUtils.getIntentSearchType(intent));
    }

    @Test
    public void getIntentIncognitoStatus_trustedIntent() {
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        mActivity,
                        EMPTY_URL,
                        IntentOrigin.CUSTOM_TAB,
                        null,
                        /* isIncognito= */ true);

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertTrue(SearchActivityUtils.getIntentIncognitoStatus(intent));
    }

    @Test
    public void getIntentIncognitoStatus_untrustedIntent() {
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        mActivity,
                        EMPTY_URL,
                        IntentOrigin.CUSTOM_TAB,
                        null,
                        /* isIncognito= */ true);

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertFalse(SearchActivityUtils.getIntentIncognitoStatus(intent));
    }

    @Test
    public void getIntentUrl_forNullUrl() {
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        mActivity, null, IntentOrigin.CUSTOM_TAB, null, /* isIncognito= */ false);
        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertNull(SearchActivityUtils.getIntentUrl(intent));
        // Remove trust
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertNull(SearchActivityUtils.getIntentUrl(intent));
    }

    @Test
    public void getIntentUrl_forEmptyUrl() {
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        mActivity,
                        GURL.emptyGURL(),
                        IntentOrigin.CUSTOM_TAB,
                        null,
                        /* isIncognito= */ false);
        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertNull(SearchActivityUtils.getIntentUrl(intent));
        // Remove trust
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertNull(SearchActivityUtils.getIntentUrl(intent));
    }

    @Test
    public void getIntentUrl_forInvalidUrl() {
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        mActivity,
                        new GURL("abcd"),
                        IntentOrigin.CUSTOM_TAB,
                        null,
                        /* isIncognito= */ false);
        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertNull(SearchActivityUtils.getIntentUrl(intent));
        // Remove trust
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertNull(SearchActivityUtils.getIntentUrl(intent));
    }

    @Test
    public void getIntentUrl_forValidUrl() {
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        mActivity,
                        new GURL("https://abc.xyz"),
                        IntentOrigin.CUSTOM_TAB,
                        null,
                        /* isIncognito= */ false);
        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertEquals("https://abc.xyz/", SearchActivityUtils.getIntentUrl(intent).getSpec());
        // Remove trust
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertNull(SearchActivityUtils.getIntentUrl(intent));
    }

    @Test
    public void getIntentSearchType_emptyPackageName() {
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        mActivity, GOOD_URL, IntentOrigin.CUSTOM_TAB, "", /* isIncognito= */ false);

        var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertEquals(IntentOrigin.CUSTOM_TAB, SearchActivityUtils.getIntentOrigin(intent));
        assertNull(SearchActivityUtils.getReferrer(intent));

        // Remove trust
        intent.removeExtra(IntentUtils.TRUSTED_APPLICATION_CODE_EXTRA);
        assertNull(SearchActivityUtils.getReferrer(intent));
    }

    @Test
    public void getIntentSearchType_nullPackageName() {
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        mActivity,
                        GOOD_URL,
                        IntentOrigin.CUSTOM_TAB,
                        null,
                        /* isIncognito= */ false);

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
            new SearchActivityClientImpl()
                    .requestOmniboxForResult(
                            mActivity,
                            GOOD_URL,
                            IntentOrigin.CUSTOM_TAB,
                            testCase,
                            /* isIncognito= */ false);
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
            new SearchActivityClientImpl()
                    .requestOmniboxForResult(
                            mActivity,
                            GOOD_URL,
                            IntentOrigin.CUSTOM_TAB,
                            testCase,
                            /* isIncognito= */ false);
            var intent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
            // Referrer will likely be stripped by the Client part...
            assertNull(IntentUtils.safeGetStringExtra(intent, SearchActivityExtras.EXTRA_REFERRER));

            // ... so let's simulate scenario where it's a custom origin:
            intent.putExtra(SearchActivityExtras.EXTRA_REFERRER, testCase);
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

    @Test
    public void createIntentForStartActivity_fromUntrustedSource() {
        Activity untrustedActivity = spy(Robolectric.buildActivity(Activity.class).setup().get());
        doReturn("com.some.app").when(untrustedActivity).getPackageName();
        var intent =
                SearchActivityUtils.createIntentForStartActivity(
                        untrustedActivity, getLoadUrlParamsBuilder().build());
        assertNull(intent);
    }

    @Test
    public void createIntentForStartActivity_fromSelf() {
        var intent =
                SearchActivityUtils.createIntentForStartActivity(
                        mActivity, getLoadUrlParamsBuilder().build());

        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT,
                intent.getFlags());
        assertEquals(Uri.parse("https://abc.xyz/"), intent.getData());
        assertTrue(intent.getBooleanExtra(SearchActivity.EXTRA_FROM_SEARCH_ACTIVITY, false));
        assertEquals(ChromeLauncherActivity.class.getName(), intent.getComponent().getClassName());
    }
}
