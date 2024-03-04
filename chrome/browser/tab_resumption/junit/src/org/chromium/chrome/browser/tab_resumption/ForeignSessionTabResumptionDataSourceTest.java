// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionCallback;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.tab_resumption.ForeignSessionTabResumptionDataSource.DataChangedObserver;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.SyncService.SyncStateChangedListener;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;
import java.util.concurrent.TimeUnit;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ForeignSessionTabResumptionDataSourceTest extends TestSupport {
    @Mock private SigninManager mSigninManager;
    @Mock private IdentityManager mIdentityManager;
    @Mock private SyncService mSyncService;
    @Mock private ForeignSessionHelper mForeignSessionHelper;

    @Captor private ArgumentCaptor<ForeignSessionCallback> mForeignSessionCallbackCaptor;
    @Captor private ArgumentCaptor<SignInStateObserver> mSignInStateObserverCaptor;
    @Captor private ArgumentCaptor<SyncStateChangedListener> mSyncStateChangedListenerCaptor;

    private long mFakeTime;
    private List<ForeignSession> mFakeSuggestions;

    private ForeignSessionTabResumptionDataSource mDataSource;
    ForeignSessionCallback mForeignSessionCallback;

    private int mDataChangedCounter;
    private boolean mLastIsPermissionUpdate;
    private DataChangedObserver mDataChangedObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mFakeTime = CURRENT_TIME_MS;
        mDataChangedObserver =
                (boolean isPermissionUpdate) -> {
                    ++mDataChangedCounter;
                    mLastIsPermissionUpdate = isPermissionUpdate;
                };
    }

    @After
    public void tearDown() {
        if (mDataSource != null) {
            mDataSource.removeObserver(mDataChangedObserver);
            mDataSource.destroy();
        }
    }

    @Test
    @SmallTest
    public void testMainFlow() {
        when(mForeignSessionHelper.getForeignSessions()).thenReturn(makeForeignSessionsA());

        createDataSource(/* isSignedIn= */ true, /* isSynced= */ true);
        Assert.assertTrue(mDataSource.canUseData());

        // Load initial suggestions.
        List<SuggestionEntry> suggestions1 = mDataSource.getSuggestions();
        verify(mForeignSessionHelper, times(1)).triggerSessionSync();
        expectFilteredSortedSuggestionsA(suggestions1);
        Assert.assertEquals(0, mDataChangedCounter);

        // Load suggestions again: There should be no change.
        List<SuggestionEntry> suggestions2 = mDataSource.getSuggestions();
        verify(mForeignSessionHelper, times(2)).triggerSessionSync();
        expectFilteredSortedSuggestionsA(suggestions2);
        Assert.assertEquals(0, mDataChangedCounter);

        // 3s elapses, change ForeignSession data, trigger update.
        mFakeTime += TimeUnit.SECONDS.toMillis(3);
        when(mForeignSessionHelper.getForeignSessions()).thenReturn(makeForeignSessionsB());
        mForeignSessionCallback.onUpdated();

        // Check data update callback.
        Assert.assertEquals(1, mDataChangedCounter);
        Assert.assertFalse(mLastIsPermissionUpdate);

        // Load suggestions, which should have changed.
        List<SuggestionEntry> suggestions3 = mDataSource.getSuggestions();
        verify(mForeignSessionHelper, times(3)).triggerSessionSync();
        expectFilteredSortedSuggestionsB(suggestions3);
    }

    @Test
    @SmallTest
    public void testPermissionChange() {
        when(mForeignSessionHelper.getForeignSessions()).thenReturn(makeForeignSessionsA());

        createDataSource(/* isSignedIn= */ true, /* isSynced= */ true);
        Assert.assertTrue(mDataSource.canUseData());

        // Load initial suggestions.
        List<SuggestionEntry> suggestions1 = mDataSource.getSuggestions();
        verify(mForeignSessionHelper, times(1)).triggerSessionSync();
        expectFilteredSortedSuggestionsA(suggestions1);
        Assert.assertEquals(0, mDataChangedCounter);

        // Disable sync, check data update callback.
        toggleIsSyncedThenNotify(false);
        Assert.assertEquals(1, mDataChangedCounter);
        Assert.assertTrue(mLastIsPermissionUpdate);
        Assert.assertFalse(mDataSource.canUseData());

        // Load suggestions: There is none.
        List<SuggestionEntry> suggestions2 = mDataSource.getSuggestions();
        verify(mForeignSessionHelper, times(1)).triggerSessionSync();
        expectEmptySuggestions(suggestions2);

        // Re-enable sync, check data update callback.
        toggleIsSyncedThenNotify(true);
        Assert.assertEquals(2, mDataChangedCounter);
        Assert.assertTrue(mLastIsPermissionUpdate);
        Assert.assertTrue(mDataSource.canUseData());

        // Suggestions are available again.
        List<SuggestionEntry> suggestions3 = mDataSource.getSuggestions();
        verify(mForeignSessionHelper, times(2)).triggerSessionSync();
        expectFilteredSortedSuggestionsA(suggestions3);
        Assert.assertEquals(2, mDataChangedCounter);

        // Log out, check data update callback.
        toggleIsSignedInThenNotify(false);
        Assert.assertEquals(3, mDataChangedCounter);
        Assert.assertTrue(mLastIsPermissionUpdate);
        Assert.assertFalse(mDataSource.canUseData());

        // Load suggestions: There is none.
        List<SuggestionEntry> suggestions4 = mDataSource.getSuggestions();
        verify(mForeignSessionHelper, times(2)).triggerSessionSync();
        expectEmptySuggestions(suggestions4);

        // Re-log in, check data update callback.
        toggleIsSignedInThenNotify(true);
        Assert.assertEquals(4, mDataChangedCounter);
        Assert.assertTrue(mLastIsPermissionUpdate);
        Assert.assertTrue(mDataSource.canUseData());

        // Suggestions are available again.
        List<SuggestionEntry> suggestions5 = mDataSource.getSuggestions();
        verify(mForeignSessionHelper, times(3)).triggerSessionSync();
        expectFilteredSortedSuggestionsA(suggestions5);
        Assert.assertEquals(4, mDataChangedCounter);
    }

    @Test
    @SmallTest
    public void testInitiallyNotSignedIn() {
        when(mForeignSessionHelper.getForeignSessions()).thenReturn(makeForeignSessionsA());

        // Initially not signed in, and sync is off.
        createDataSource(/* isSignedIn= */ false, /* isSynced= */ false);
        Assert.assertFalse(mDataSource.canUseData());

        // Load suggestions: There is none.
        List<SuggestionEntry> suggestions1 = mDataSource.getSuggestions();
        verify(mForeignSessionHelper, times(0)).triggerSessionSync();
        expectEmptySuggestions(suggestions1);
        Assert.assertEquals(0, mDataChangedCounter);

        // Sign in.
        toggleIsSignedInThenNotify(true);
        Assert.assertEquals(1, mDataChangedCounter);
        Assert.assertTrue(mLastIsPermissionUpdate);
        Assert.assertFalse(mDataSource.canUseData());

        // Load suggestions: Still none, since sync is off.
        List<SuggestionEntry> suggestions2 = mDataSource.getSuggestions();
        verify(mForeignSessionHelper, times(0)).triggerSessionSync();
        expectEmptySuggestions(suggestions2);
        Assert.assertEquals(1, mDataChangedCounter);

        // Enable sync.
        toggleIsSyncedThenNotify(true);
        Assert.assertEquals(2, mDataChangedCounter);
        Assert.assertTrue(mLastIsPermissionUpdate);
        Assert.assertTrue(mDataSource.canUseData());

        // Load suggestions: Now things should work.
        List<SuggestionEntry> suggestions3 = mDataSource.getSuggestions();
        verify(mForeignSessionHelper, times(1)).triggerSessionSync();
        expectFilteredSortedSuggestionsA(suggestions3);
        Assert.assertEquals(2, mDataChangedCounter);
    }

    private void createDataSource(boolean isSignedIn, boolean isSynced) {
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(isSignedIn);
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(isSynced);
        mDataSource =
                new ForeignSessionTabResumptionDataSource(
                        /* signinManager= */ mSigninManager,
                        /* identityManager= */ mIdentityManager,
                        /* syncService= */ mSyncService,
                        /* foreignSessionHelper= */ mForeignSessionHelper) {
                    @Override
                    long getCurrentTimeMs() {
                        return mFakeTime;
                    }
                };
        mDataSource.addObserver(mDataChangedObserver);

        verify(mSigninManager).addSignInStateObserver(mSignInStateObserverCaptor.capture());
        verify(mSyncService).addSyncStateChangedListener(mSyncStateChangedListenerCaptor.capture());
        verify(mForeignSessionHelper)
                .setOnForeignSessionCallback(mForeignSessionCallbackCaptor.capture());
        Assert.assertEquals(mDataSource, mSignInStateObserverCaptor.getValue());
        Assert.assertEquals(mDataSource, mSyncStateChangedListenerCaptor.getValue());
        mForeignSessionCallback = mForeignSessionCallbackCaptor.getValue();
        Assert.assertNotNull(mForeignSessionCallback);
    }

    private void toggleIsSignedInThenNotify(boolean isSignedIn) {
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(isSignedIn);
        // For simplicity, call handlers directly instead of using `mSigninManager`.
        if (isSignedIn) {
            mSignInStateObserverCaptor.getValue().onSignedIn();
        } else {
            mSignInStateObserverCaptor.getValue().onSignedOut();
        }
    }

    private void toggleIsSyncedThenNotify(boolean isSynced) {
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(isSynced);
        // For simplicity, call handlers directly instead of using `mSyncService`.
        mSyncStateChangedListenerCaptor.getValue().syncStateChanged();
    }

    private void checkSuggestionListIsSorted(List<SuggestionEntry> suggestions) {
        SuggestionEntry prevEntry = null;
        for (SuggestionEntry entry : suggestions) {
            if (prevEntry != null) {
                Assert.assertTrue(prevEntry.compareTo(entry) < 0);
            }
            prevEntry = entry;
        }
    }

    private void expectFilteredSortedSuggestionsA(@Nullable List<SuggestionEntry> suggestions) {
        // There are 7 tabs total, but TAB3 is invalid, and TAB2 is stale, resulting in 5.
        Assert.assertEquals(5, suggestions.size());
        checkSuggestionListIsSorted(suggestions);
        // Just check values for the first entry, which should be TAB6, being the most recent.
        SuggestionEntry firstEntry = suggestions.get(0);
        Assert.assertEquals("My Tablet", firstEntry.sourceName);
        Assert.assertEquals(JUnitTestGURLs.INITIAL_URL, firstEntry.url);
        Assert.assertEquals("Initial", firstEntry.title);
        Assert.assertEquals(makeTimestamp(8, 0, 0), firstEntry.lastActiveTime);
        Assert.assertEquals(106, firstEntry.id);
    }

    private void expectFilteredSortedSuggestionsB(@Nullable List<SuggestionEntry> suggestions) {
        // Only TAB5 and TAB7 are open, and they got selected.
        Assert.assertEquals(2, suggestions.size());
        checkSuggestionListIsSorted(suggestions);
        // Just check values for the first entry, which should be TAB5.
        SuggestionEntry firstEntry = suggestions.get(0);
        Assert.assertEquals("My Tablet", firstEntry.sourceName);
        Assert.assertEquals(JUnitTestGURLs.MAPS_URL, firstEntry.url);
        Assert.assertEquals("Maps", firstEntry.title);
        Assert.assertEquals(makeTimestamp(4, 0, 0), firstEntry.lastActiveTime);
        Assert.assertEquals(105, firstEntry.id);
    }

    private void expectEmptySuggestions(@Nullable List<SuggestionEntry> suggestions) {
        Assert.assertEquals(0, suggestions.size());
    }
}
