// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.SessionHandler;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;

/** Unit tests for {@link LaunchIntentDispatcher}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LaunchIntentDispatcherTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private CustomTabsConnection mCustomTabsConnection;
    @Mock private SessionDataHolder mSessionDataHolder;
    @Mock private SessionHandler mSessionHandler;
    @Mock private ActivityManager mActivityManager;

    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        CustomTabsConnection.setInstanceForTesting(mCustomTabsConnection);
        SessionDataHolder.setInstanceForTesting(mSessionDataHolder);
    }

    @Test
    public void testDispatchToCustomTabActivity_DelegatesToExistingHandler() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(
                androidx.browser.customtabs.CustomTabsIntent.EXTRA_SESSION,
                (android.os.IBinder) null);

        doReturn(mSessionHandler).when(mSessionDataHolder).getActiveHandlerForIntent(any());
        doReturn(true).when(mSessionHandler).handleIntent(any());
        int taskId = 123;
        doReturn(taskId).when(mSessionHandler).getTaskId();

        Activity spyActivity = spy(mActivity);
        doReturn(mActivityManager).when(spyActivity).getSystemService(Context.ACTIVITY_SERVICE);

        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(mSessionHandler).handleIntent(intent);
        verify(mActivityManager).moveTaskToFront(eq(taskId), anyInt());
    }

    @Test
    public void testDispatchToCustomTabActivity_StartsNewActivityIfNoHandler() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(
                androidx.browser.customtabs.CustomTabsIntent.EXTRA_SESSION,
                (android.os.IBinder) null);

        doReturn(null).when(mSessionDataHolder).getActiveHandlerForIntent(any());

        Activity spyActivity = spy(mActivity);

        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).startActivity(any(), any());
        verifyNoInteractions(mSessionHandler);
    }
}
