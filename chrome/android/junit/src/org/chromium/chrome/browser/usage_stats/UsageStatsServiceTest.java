// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link UsageStatsService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UsageStatsServiceTest {
    @Mock private UsageStatsBridge.Natives mBridgeNatives;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private DigitalWellbeingClient mClient;

    private UsageStatsService mService;
    private AutoCloseable mCloseable;

    @Before
    public void setUp() {
        mCloseable = MockitoAnnotations.openMocks(this);
        UsageStatsBridgeJni.setInstanceForTesting(mBridgeNatives);
        UserPrefs.setPrefServiceForTesting(mPrefService);
        ServiceLoaderUtil.setInstanceForTesting(DigitalWellbeingClient.class, mClient);

        // Mock basic bridge JNI calls.
        when(mBridgeNatives.init(any(), any())).thenReturn(1L);
        doAnswer(
                        invocation -> {
                            Callback<String[]> callback = invocation.getArgument(1);
                            callback.onResult(new String[0]);
                            return null;
                        })
                .when(mBridgeNatives)
                .getAllSuspensions(eq(1L), any());
        doAnswer(
                        invocation -> {
                            Callback<Map<String, String>> callback = invocation.getArgument(1);
                            callback.onResult(new HashMap<>());
                            return null;
                        })
                .when(mBridgeNatives)
                .getAllTokenMappings(eq(1L), any());

        // Mock event deletion JNI calls to avoid promise hangs.
        doAnswer(
                        invocation -> {
                            Callback<Boolean> callback = invocation.getArgument(1);
                            callback.onResult(true);
                            return null;
                        })
                .when(mBridgeNatives)
                .deleteAllEvents(eq(1L), any());
        doAnswer(
                        invocation -> {
                            Callback<Boolean> callback = invocation.getArgument(3);
                            callback.onResult(true);
                            return null;
                        })
                .when(mBridgeNatives)
                .deleteEventsInRange(eq(1L), anyLong(), anyLong(), any());
        doAnswer(
                        invocation -> {
                            Callback<Boolean> callback = invocation.getArgument(2);
                            callback.onResult(true);
                            return null;
                        })
                .when(mBridgeNatives)
                .deleteEventsWithMatchingDomains(eq(1L), any(), any());
    }

    @After
    public void tearDown() throws Exception {
        mCloseable.close();
    }

    private void createService() {
        mService = new UsageStatsService(mProfile);
    }

    @Test
    public void testOnAllHistoryDeleted_OptedIn_CallsClient() {
        when(mPrefService.getBoolean(Pref.USAGE_STATS_ENABLED)).thenReturn(true);
        createService();

        mService.onAllHistoryDeleted();

        verify(mClient).notifyAllHistoryCleared();
    }

    @Test
    public void testOnAllHistoryDeleted_OptedOut_DoesNotCallClient() {
        when(mPrefService.getBoolean(Pref.USAGE_STATS_ENABLED)).thenReturn(false);
        createService();

        mService.onAllHistoryDeleted();

        verify(mClient, never()).notifyAllHistoryCleared();
    }

    @Test
    public void testOnHistoryDeletedInRange_OptedIn_CallsClient() {
        when(mPrefService.getBoolean(Pref.USAGE_STATS_ENABLED)).thenReturn(true);
        createService();

        mService.onHistoryDeletedInRange(1000L, 2000L);

        verify(mClient).notifyHistoryDeletion(eq(1000L), anyLong());
    }

    @Test
    public void testOnHistoryDeletedInRange_OptedOut_DoesNotCallClient() {
        when(mPrefService.getBoolean(Pref.USAGE_STATS_ENABLED)).thenReturn(false);
        createService();

        mService.onHistoryDeletedInRange(1000L, 2000L);

        verify(mClient, never()).notifyHistoryDeletion(anyLong(), anyLong());
    }

    @Test
    public void testOnHistoryDeletedForDomains_OptedIn_CallsClient() {
        when(mPrefService.getBoolean(Pref.USAGE_STATS_ENABLED)).thenReturn(true);
        createService();

        mService.onHistoryDeletedForDomains(Arrays.asList("google.com", "chromium.org"));

        verify(mClient).notifyHistoryDeletion(Arrays.asList("google.com", "chromium.org"));
    }

    @Test
    public void testOnHistoryDeletedForDomains_OptedOut_DoesNotCallClient() {
        when(mPrefService.getBoolean(Pref.USAGE_STATS_ENABLED)).thenReturn(false);
        createService();

        mService.onHistoryDeletedForDomains(Arrays.asList("google.com", "chromium.org"));

        verify(mClient, never()).notifyHistoryDeletion(anyList());
    }
}
