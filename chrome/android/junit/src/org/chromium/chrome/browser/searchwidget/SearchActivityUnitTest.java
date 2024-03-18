// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient.IntentOrigin;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {SearchActivityUnitTest.ShadowSearchActivityUtils.class})
public class SearchActivityUnitTest {
    private static final OmniboxLoadUrlParams LOAD_URL_PARAMS_SIMPLE =
            new OmniboxLoadUrlParams.Builder("https://abc.xyz", PageTransition.TYPED).build();

    // SearchActivityUtils call intercepting mock.
    private interface TestSearchActivityUtils {
        @IntentOrigin
        int getIntentOrigin(Intent intent);

        void resolveOmniboxRequestForResult(Activity activity, OmniboxLoadUrlParams params);

        GURL getIntentUrl(Intent intent);
    }

    // Shadow forwarding static calls to TestSearchActivityUtils.
    @Implements(SearchActivityUtils.class)
    public static class ShadowSearchActivityUtils {
        static TestSearchActivityUtils sMockUtils;

        @Implementation
        public static @IntentOrigin int getIntentOrigin(Intent intent) {
            return sMockUtils.getIntentOrigin(intent);
        }

        @Implementation
        public static GURL getIntentUrl(Intent intent) {
            return sMockUtils.getIntentUrl(intent);
        }

        @Implementation
        public static void resolveOmniboxRequestForResult(
                Activity activity, OmniboxLoadUrlParams params) {
            sMockUtils.resolveOmniboxRequestForResult(activity, params);
        }
    }

    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    private @Mock TestSearchActivityUtils mUtils;

    private ActivityController<SearchActivity> mController;
    private SearchActivity mActivity;
    private ShadowActivity mShadowActivity;

    @Before
    public void setUp() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        mController = Robolectric.buildActivity(SearchActivity.class);
        mActivity = mController.get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mShadowActivity = shadowOf(mActivity);

        mActivity.setActivityUsableForTesting(true);
        ShadowSearchActivityUtils.sMockUtils = mUtils;
    }

    @After
    public void tearDown() {
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        FirstRunStatus.setFirstRunFlowComplete(false);
        IdentityServicesProvider.setInstanceForTests(null);
    }

    @Test
    public void loadUrl_dispatchResultToCallingActivity() {
        doReturn(IntentOrigin.CUSTOM_TAB).when(mUtils).getIntentOrigin(any());
        mActivity.handleNewIntent(new Intent());

        mActivity.loadUrl(LOAD_URL_PARAMS_SIMPLE, false);
        ArgumentCaptor<OmniboxLoadUrlParams> captor =
                ArgumentCaptor.forClass(OmniboxLoadUrlParams.class);
        verify(mUtils).resolveOmniboxRequestForResult(eq(mActivity), captor.capture());

        assertEquals("https://abc.xyz", captor.getValue().url);
        assertNull(mShadowActivity.getNextStartedActivity());
    }

    @Test
    public void loadUrl_openInChromeBrowser() {
        doReturn(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET).when(mUtils).getIntentOrigin(any());
        mActivity.handleNewIntent(new Intent());

        mActivity.loadUrl(LOAD_URL_PARAMS_SIMPLE, false);
        verify(mUtils, never()).resolveOmniboxRequestForResult(any(), any());
        assertNotNull(mShadowActivity.getNextStartedActivity());
    }

    @Test
    public void loadUrl_noActionWhenActivityIsNotReady() {
        doReturn(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET).when(mUtils).getIntentOrigin(any());
        mActivity.setActivityUsableForTesting(false);
        mActivity.handleNewIntent(new Intent());

        mActivity.loadUrl(LOAD_URL_PARAMS_SIMPLE, false);
        verify(mUtils, never()).resolveOmniboxRequestForResult(any(), any());
        assertNull(mShadowActivity.getNextStartedActivity());
    }

    @Test
    public void cancelSearch_dispatchResultToCallingActivity() {
        doReturn(IntentOrigin.CUSTOM_TAB).when(mUtils).getIntentOrigin(any());
        mActivity.handleNewIntent(new Intent());

        mActivity.cancelSearch();
        verify(mUtils).resolveOmniboxRequestForResult(mActivity, null);
    }

    @Test
    public void cancelSearch_terminateSearch() {
        doReturn(IntentOrigin.SEARCH_WIDGET).when(mUtils).getIntentOrigin(any());
        mActivity.handleNewIntent(new Intent());

        mActivity.cancelSearch();
        verify(mUtils, never()).resolveOmniboxRequestForResult(any(), any());
    }

    @Test
    public void handleNewIntent_forSearchWidget() {
        doReturn(IntentOrigin.SEARCH_WIDGET).when(mUtils).getIntentOrigin(any());
        mActivity.handleNewIntent(new Intent());

        var data = mActivity.getSearchBoxDataProvider();
        assertEquals(
                PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                data.getPageClassification(true, true));
        assertEquals(
                PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                data.getPageClassification(true, false));
    }

    @Test
    public void handleNewIntent_forQuickActionSearchWidget() {
        doReturn(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET).when(mUtils).getIntentOrigin(any());
        mActivity.handleNewIntent(new Intent());

        var data = mActivity.getSearchBoxDataProvider();
        assertEquals(
                PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE,
                data.getPageClassification(true, true));
        assertEquals(
                PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE,
                data.getPageClassification(true, false));
    }

    @Test
    public void handleNewIntent_forCustomTab() {
        doReturn(IntentOrigin.CUSTOM_TAB).when(mUtils).getIntentOrigin(any());
        mActivity.handleNewIntent(new Intent());

        var data = mActivity.getSearchBoxDataProvider();
        assertEquals(PageClassification.OTHER_VALUE, data.getPageClassification(true, true));
        assertEquals(PageClassification.OTHER_VALUE, data.getPageClassification(true, false));
    }

    @Test
    public void handleNewIntent_passIntentUrlToLocationBarData() {
        doReturn(new GURL("https://abc.xyz")).when(mUtils).getIntentUrl(any());
        mActivity.handleNewIntent(new Intent());

        var data = mActivity.getSearchBoxDataProvider();
        assertEquals("https://abc.xyz/", data.getCurrentGurl().getSpec());
    }
}
