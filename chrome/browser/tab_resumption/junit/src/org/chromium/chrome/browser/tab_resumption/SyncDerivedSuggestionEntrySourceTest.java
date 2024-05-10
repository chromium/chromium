// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.tab_resumption.SyncDerivedSuggestionEntrySource.SourceDataChangedObserver;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.SyncService.SyncStateChangedListener;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for SyncDerivedSuggestionEntrySource. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SyncDerivedSuggestionEntrySourceTest extends TestSupport {
    @Mock private SigninManager mSigninManager;
    @Mock private IdentityManager mIdentityManager;
    @Mock private SyncService mSyncService;
    @Mock private SuggestionBackend mSuggestionBackend;

    @Captor private ArgumentCaptor<Runnable> mUpdateObserverCaptor;
    @Captor private ArgumentCaptor<SignInStateObserver> mSignInStateObserverCaptor;
    @Captor private ArgumentCaptor<SyncStateChangedListener> mSyncStateChangedListenerCaptor;
    @Captor private ArgumentCaptor<Callback<List<SuggestionEntry>>> mReadCachedCallbackCaptor;

    private long mFakeTime;
    private List<ForeignSession> mFakeSuggestions;

    private SyncDerivedSuggestionEntrySource mSource;
    private Runnable mUpdateObserver;

    private int mDataChangedCounter;
    private boolean mLastIsPermissionUpdate;
    private SourceDataChangedObserver mSourceDataChangedObserver;

    private boolean mGetSuggestionsSyncCallFlag;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mFakeTime = CURRENT_TIME_MS;
        mSourceDataChangedObserver =
                (boolean isPermissionUpdate) -> {
                    ++mDataChangedCounter;
                    mLastIsPermissionUpdate = isPermissionUpdate;
                };
    }

    @After
    public void tearDown() {
        if (mSource != null) {
            mSource.removeObserver(mSourceDataChangedObserver);
            mSource.destroy();
        }
    }

    @Test
    @SmallTest
    public void testMainFlow() {
        plantSuggestionBackendReadCachedResult(makeForeignSessionSuggestionsA());

        createEntrySource(/* isSignedIn= */ true, /* isSynced= */ true);
        Assert.assertTrue(mSource.canUseData());

        // Load initial suggestions.
        List<SuggestionEntry> suggestions1 = getSuggestionsSync();
        verify(mSuggestionBackend, times(1)).triggerUpdate();
        assertSuggestionsEqual(makeForeignSessionSuggestionsA(), suggestions1);
        Assert.assertEquals(0, mDataChangedCounter);

        // Load suggestions again: There should be no change.
        List<SuggestionEntry> suggestions2 = getSuggestionsSync();
        verify(mSuggestionBackend, times(2)).triggerUpdate();
        assertSuggestionsEqual(makeForeignSessionSuggestionsA(), suggestions2);
        Assert.assertEquals(0, mDataChangedCounter);

        // 3s elapses, change ForeignSession data, trigger update.
        mFakeTime += TimeUnit.SECONDS.toMillis(3);
        plantSuggestionBackendReadCachedResult(makeForeignSessionSuggestionsB());
        mUpdateObserver.run();

        // Check data update callback.
        Assert.assertEquals(1, mDataChangedCounter);
        Assert.assertFalse(mLastIsPermissionUpdate);

        // Load suggestions, which should have changed.
        List<SuggestionEntry> suggestions3 = getSuggestionsSync();
        verify(mSuggestionBackend, times(3)).triggerUpdate();
        assertSuggestionsEqual(makeForeignSessionSuggestionsB(), suggestions3);
    }

    @Test
    @SmallTest
    public void testPermissionChange() {
        plantSuggestionBackendReadCachedResult(makeForeignSessionSuggestionsA());

        createEntrySource(/* isSignedIn= */ true, /* isSynced= */ true);
        Assert.assertTrue(mSource.canUseData());

        // Load initial suggestions.
        List<SuggestionEntry> suggestions1 = getSuggestionsSync();
        verify(mSuggestionBackend, times(1)).triggerUpdate();
        assertSuggestionsEqual(makeForeignSessionSuggestionsA(), suggestions1);
        Assert.assertEquals(0, mDataChangedCounter);

        // Disable sync, check data update callback.
        toggleIsSyncedThenNotify(false);
        Assert.assertEquals(1, mDataChangedCounter);
        Assert.assertTrue(mLastIsPermissionUpdate);
        Assert.assertFalse(mSource.canUseData());

        // Load suggestions: There is none.
        List<SuggestionEntry> suggestions2 = getSuggestionsSync();
        verify(mSuggestionBackend, times(1)).triggerUpdate();
        assertEmptySuggestions(suggestions2);

        // Re-enable sync, check data update callback.
        toggleIsSyncedThenNotify(true);
        Assert.assertEquals(2, mDataChangedCounter);
        Assert.assertTrue(mLastIsPermissionUpdate);
        Assert.assertTrue(mSource.canUseData());

        // Suggestions are available again.
        List<SuggestionEntry> suggestions3 = getSuggestionsSync();
        verify(mSuggestionBackend, times(2)).triggerUpdate();
        assertSuggestionsEqual(makeForeignSessionSuggestionsA(), suggestions3);
        Assert.assertEquals(2, mDataChangedCounter);

        // Log out, check data update callback.
        toggleIsSignedInThenNotify(false);
        Assert.assertEquals(3, mDataChangedCounter);
        Assert.assertTrue(mLastIsPermissionUpdate);
        Assert.assertFalse(mSource.canUseData());

        // Load suggestions: There is none.
        List<SuggestionEntry> suggestions4 = getSuggestionsSync();
        verify(mSuggestionBackend, times(2)).triggerUpdate();
        assertEmptySuggestions(suggestions4);

        // Re-log in, check data update callback.
        toggleIsSignedInThenNotify(true);
        Assert.assertEquals(4, mDataChangedCounter);
        Assert.assertTrue(mLastIsPermissionUpdate);
        Assert.assertTrue(mSource.canUseData());

        // Suggestions are available again.
        List<SuggestionEntry> suggestions5 = getSuggestionsSync();
        verify(mSuggestionBackend, times(3)).triggerUpdate();
        assertSuggestionsEqual(makeForeignSessionSuggestionsA(), suggestions5);
        Assert.assertEquals(4, mDataChangedCounter);
    }

    @Test
    @SmallTest
    public void testInitiallyNotSignedIn() {
        plantSuggestionBackendReadCachedResult(makeForeignSessionSuggestionsA());

        // Initially not signed in, and sync is off.
        createEntrySource(/* isSignedIn= */ false, /* isSynced= */ false);
        Assert.assertFalse(mSource.canUseData());

        // Load suggestions: There is none.
        List<SuggestionEntry> suggestions1 = getSuggestionsSync();
        verify(mSuggestionBackend, times(0)).triggerUpdate();
        assertEmptySuggestions(suggestions1);
        Assert.assertEquals(0, mDataChangedCounter);

        // Sign in.
        toggleIsSignedInThenNotify(true);
        Assert.assertEquals(1, mDataChangedCounter);
        Assert.assertTrue(mLastIsPermissionUpdate);
        Assert.assertFalse(mSource.canUseData());

        // Load suggestions: Still none, since sync is off.
        List<SuggestionEntry> suggestions2 = getSuggestionsSync();
        verify(mSuggestionBackend, times(0)).triggerUpdate();
        assertEmptySuggestions(suggestions2);
        Assert.assertEquals(1, mDataChangedCounter);

        // Enable sync.
        toggleIsSyncedThenNotify(true);
        Assert.assertEquals(2, mDataChangedCounter);
        Assert.assertTrue(mLastIsPermissionUpdate);
        Assert.assertTrue(mSource.canUseData());

        // Load suggestions: Now things should work.
        List<SuggestionEntry> suggestions3 = getSuggestionsSync();
        verify(mSuggestionBackend, times(1)).triggerUpdate();
        assertSuggestionsEqual(makeForeignSessionSuggestionsA(), suggestions3);
        Assert.assertEquals(2, mDataChangedCounter);
    }

    private void createEntrySource(boolean isSignedIn, boolean isSynced) {
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(isSignedIn);
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(isSynced);
        mSource =
                new SyncDerivedSuggestionEntrySource(
                        /* signinManager= */ mSigninManager,
                        /* identityManager= */ mIdentityManager,
                        /* syncService= */ mSyncService,
                        /* foreignSessionHelper= */ mSuggestionBackend) {
                    @Override
                    long getCurrentTimeMs() {
                        return mFakeTime;
                    }
                };
        mSource.addObserver(mSourceDataChangedObserver);

        verify(mSigninManager).addSignInStateObserver(mSignInStateObserverCaptor.capture());
        verify(mSyncService).addSyncStateChangedListener(mSyncStateChangedListenerCaptor.capture());
        verify(mSuggestionBackend).setUpdateObserver(mUpdateObserverCaptor.capture());
        Assert.assertEquals(mSource, mSignInStateObserverCaptor.getValue());
        Assert.assertEquals(mSource, mSyncStateChangedListenerCaptor.getValue());
        mUpdateObserver = mUpdateObserverCaptor.getValue();
        Assert.assertNotNull(mUpdateObserver);
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

    /**
     * Plants callback-passed results for mSuggestionBackend.readCached(), similar to
     * when(...).thenReturn(...) and less committal than using ArgumentCaptor.
     */
    private void plantSuggestionBackendReadCachedResult(List<SuggestionEntry> suggestions) {
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            ((Callback<List<SuggestionEntry>>) invocation.getArguments()[0])
                                    .onResult(suggestions);
                            return null;
                        })
                .when(mSuggestionBackend)
                .readCached(any(Callback.class));
    }

    /** Adapts `mSource.getSuggestions()` call to return results synchronously. */
    private List<SuggestionEntry> getSuggestionsSync() {
        List<SuggestionEntry> ret = new ArrayList<SuggestionEntry>();
        mGetSuggestionsSyncCallFlag = false;
        // The test setup ensures that the passed lambda is eagerly called.
        mSource.getSuggestions(
                (List<SuggestionEntry> suggestions) -> {
                    ret.addAll(suggestions);
                    mGetSuggestionsSyncCallFlag = true;
                });
        assert mGetSuggestionsSyncCallFlag;
        return ret;
    }
}
