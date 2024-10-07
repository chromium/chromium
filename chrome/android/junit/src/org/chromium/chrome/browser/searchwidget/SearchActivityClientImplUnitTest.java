// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;

import android.app.Activity;
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

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ResourceRequestBodyJni;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

@RunWith(BaseRobolectricTestRunner.class)
public class SearchActivityClientImplUnitTest {
    // Placeholder Activity class that guarantees the PackageName is valid for IntentUtils.
    private static class TestActivity extends Activity {}

    private static final GURL GOOD_URL = new GURL("https://abc.xyz");
    private static final GURL EMPTY_URL = GURL.emptyGURL();
    private static final ComponentName COMPONENT_TRUSTED =
            new ComponentName(ContextUtils.getApplicationContext(), SearchActivity.class);

    private Activity mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    public @Rule JniMocker mJniMocker = new JniMocker();
    private @Mock ResourceRequestBodyJni mResourceRequestBodyJni;

    @Before
    public void setUp() {
        mJniMocker.mock(ResourceRequestBodyJni.TEST_HOOKS, mResourceRequestBodyJni);
        doAnswer(i -> i.getArgument(0))
                .when(mResourceRequestBodyJni)
                .createResourceRequestBodyFromBytes(any());
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

        SearchActivityClient client = new SearchActivityClientImpl();
        for (int origin : origins) {
            String action =
                    String.format(
                            SearchActivityClientImpl.ACTION_SEARCH_FORMAT, origin, SearchType.TEXT);

            // null URL
            var intent = client.createIntent(mActivity, origin, null, SearchType.TEXT);
            assertEquals(action, intent.getAction());
            assertNull(intent.getStringExtra(SearchActivityExtras.EXTRA_CURRENT_URL));
            assertEquals(SearchType.TEXT, SearchActivityUtils.getIntentSearchType(intent));
            assertEquals(origin, SearchActivityUtils.getIntentOrigin(intent));

            // non-null URL
            intent =
                    client.createIntent(
                            mActivity, origin, new GURL("http://abc.xyz"), SearchType.TEXT);
            assertEquals(action, intent.getAction());
            assertEquals(
                    "http://abc.xyz/",
                    intent.getStringExtra(SearchActivityExtras.EXTRA_CURRENT_URL));
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

        SearchActivityClient client = new SearchActivityClientImpl();
        for (int origin : origins) {
            String action =
                    String.format(
                            SearchActivityClientImpl.ACTION_SEARCH_FORMAT,
                            origin,
                            SearchType.VOICE);

            // null URL
            var intent = client.createIntent(mActivity, origin, null, SearchType.VOICE);
            assertEquals(action, intent.getAction());
            assertNull(intent.getStringExtra(SearchActivityExtras.EXTRA_CURRENT_URL));
            assertEquals(SearchType.VOICE, SearchActivityUtils.getIntentSearchType(intent));
            assertEquals(origin, SearchActivityUtils.getIntentOrigin(intent));

            // non-null URL
            intent =
                    client.createIntent(
                            mActivity, origin, new GURL("http://abc.xyz"), SearchType.VOICE);
            assertEquals(action, intent.getAction());
            assertEquals(
                    "http://abc.xyz/",
                    intent.getStringExtra(SearchActivityExtras.EXTRA_CURRENT_URL));
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

        SearchActivityClient client = new SearchActivityClientImpl();
        for (int origin : origins) {
            String action =
                    String.format(
                            SearchActivityClientImpl.ACTION_SEARCH_FORMAT, origin, SearchType.LENS);

            // null URL
            var intent = client.createIntent(mActivity, origin, null, SearchType.LENS);
            assertEquals(action, intent.getAction());
            assertNull(intent.getStringExtra(SearchActivityExtras.EXTRA_CURRENT_URL));
            assertEquals(SearchType.LENS, SearchActivityUtils.getIntentSearchType(intent));
            assertEquals(origin, SearchActivityUtils.getIntentOrigin(intent));

            // non-null URL
            intent =
                    client.createIntent(
                            mActivity, origin, new GURL("http://abc.xyz"), SearchType.LENS);
            assertEquals(action, intent.getAction());
            assertEquals(
                    "http://abc.xyz/",
                    intent.getStringExtra(SearchActivityExtras.EXTRA_CURRENT_URL));
            assertEquals(SearchType.LENS, SearchActivityUtils.getIntentSearchType(intent));
            assertEquals(origin, SearchActivityUtils.getIntentOrigin(intent));
        }
    }

    @Test
    public void buildTrustedIntent_appliesExpectedAction() {
        var intent = SearchActivityClientImpl.buildTrustedIntent(mActivity, "abcd");
        assertEquals("abcd", intent.getAction());

        intent = SearchActivityClientImpl.buildTrustedIntent(mActivity, "1234");
        assertEquals("1234", intent.getAction());
    }

    @Test
    public void buildTrustedIntent_addressesSearchActivity() {
        var intent = SearchActivityClientImpl.buildTrustedIntent(mActivity, "a");
        assertEquals(
                intent.getComponent().getClassName().toString(), SearchActivity.class.getName());
    }

    @Test
    public void buildTrustedIntent_intentIsTrusted() {
        var intent = SearchActivityClientImpl.buildTrustedIntent(mActivity, "a");
        assertTrue(IntentUtils.isTrustedIntentFromSelf(intent));
    }

    @Test
    public void requestOmniboxForResult_noActionWhenActivityIsNull() {
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        null, EMPTY_URL, IntentOrigin.CUSTOM_TAB, null, /* isIncognito= */ false);
    }

    @Test
    public void requestOmniboxForResult_propagatesCurrentUrl() {
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        mActivity,
                        GOOD_URL,
                        IntentOrigin.CUSTOM_TAB,
                        null,
                        /* isIncognito= */ false);

        var intentForResult = Shadows.shadowOf(mActivity).getNextStartedActivityForResult();

        assertEquals(
                IntentUtils.safeGetStringExtra(
                        intentForResult.intent, SearchActivityExtras.EXTRA_CURRENT_URL),
                GOOD_URL.getSpec());
        assertEquals(SearchActivityClientImpl.OMNIBOX_REQUEST_CODE, intentForResult.requestCode);
    }

    @Test
    public void requestOmniboxForResult_acceptsEmptyUrl() {
        // This is technically an invalid case. The test verifies we still do the right thing.
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        mActivity,
                        EMPTY_URL,
                        IntentOrigin.CUSTOM_TAB,
                        null,
                        /* isIncognito= */ false);

        var intentForResult = Shadows.shadowOf(mActivity).getNextStartedActivityForResult();

        assertTrue(
                IntentUtils.safeHasExtra(
                        intentForResult.intent, SearchActivityExtras.EXTRA_CURRENT_URL));
        assertTrue(
                TextUtils.isEmpty(
                        IntentUtils.safeGetStringExtra(
                                intentForResult.intent, SearchActivityExtras.EXTRA_CURRENT_URL)));
        assertEquals(SearchActivityClientImpl.OMNIBOX_REQUEST_CODE, intentForResult.requestCode);
    }

    @Test
    public void requestOmniboxForResult_propagatesIncognitoStatus() {
        new SearchActivityClientImpl()
                .requestOmniboxForResult(
                        mActivity,
                        GOOD_URL,
                        IntentOrigin.CUSTOM_TAB,
                        null,
                        /* isIncognito= */ true);

        var intentForResult = Shadows.shadowOf(mActivity).getNextStartedActivityForResult();

        assertTrue(
                IntentUtils.safeHasExtra(
                        intentForResult.intent, SearchActivityExtras.EXTRA_IS_INCOGNITO));
        assertEquals(
                IntentUtils.safeGetBooleanExtra(
                        intentForResult.intent, SearchActivityExtras.EXTRA_IS_INCOGNITO, false),
                true);
        assertEquals(SearchActivityClientImpl.OMNIBOX_REQUEST_CODE, intentForResult.requestCode);
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
                SearchActivityClientImpl.isOmniboxResult(
                        SearchActivityClientImpl.OMNIBOX_REQUEST_CODE, intent));
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
                SearchActivityClientImpl.isOmniboxResult(
                        SearchActivityClientImpl.OMNIBOX_REQUEST_CODE - 1, intent));
        assertFalse(SearchActivityClientImpl.isOmniboxResult(0, intent));
        assertFalse(SearchActivityClientImpl.isOmniboxResult(~0, intent));
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
                SearchActivityClientImpl.isOmniboxResult(
                        SearchActivityClientImpl.OMNIBOX_REQUEST_CODE, intent));
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
                SearchActivityClientImpl.isOmniboxResult(
                        SearchActivityClientImpl.OMNIBOX_REQUEST_CODE, intent));
    }

    @Test
    public void getOmniboxResult_withInvalidUrl() {
        var intent = new Intent();
        intent.setComponent(COMPONENT_TRUSTED);
        intent.setData(Uri.parse("a b"));
        IntentUtils.addTrustedIntentExtras(intent);
        assertNull(
                SearchActivityClientImpl.getOmniboxResult(
                        SearchActivityClientImpl.OMNIBOX_REQUEST_CODE, Activity.RESULT_OK, intent));
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
                SearchActivityClientImpl.getOmniboxResult(
                        SearchActivityClientImpl.OMNIBOX_REQUEST_CODE, Activity.RESULT_OK, intent);

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
                SearchActivityClientImpl.getOmniboxResult(
                        SearchActivityClientImpl.OMNIBOX_REQUEST_CODE, Activity.RESULT_OK, intent);

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
                SearchActivityClientImpl.getOmniboxResult(
                        SearchActivityClientImpl.OMNIBOX_REQUEST_CODE, Activity.RESULT_OK, intent);

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
                SearchActivityClientImpl.getOmniboxResult(
                        SearchActivityClientImpl.OMNIBOX_REQUEST_CODE, Activity.RESULT_OK, intent);

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
                SearchActivityClientImpl.getOmniboxResult(
                        SearchActivityClientImpl.OMNIBOX_REQUEST_CODE, Activity.RESULT_OK, intent);

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
                SearchActivityClientImpl.getOmniboxResult(
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
                SearchActivityClientImpl.getOmniboxResult(
                        SearchActivityClientImpl.OMNIBOX_REQUEST_CODE,
                        Activity.RESULT_CANCELED,
                        intent));
    }
}
