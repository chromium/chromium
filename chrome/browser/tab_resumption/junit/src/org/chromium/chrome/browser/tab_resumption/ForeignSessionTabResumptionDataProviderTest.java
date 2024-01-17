// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.ArgumentMatchers.anyInt;
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
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.SyncService.SyncStateChangedListener;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ForeignSessionTabResumptionDataProviderTest {
    private static final long CURRENT_TIME_MS = 1705000000000L + 24L * 60L * 60L * 1000L;

    private static final ForeignSessionTab TAB1 =
            new ForeignSessionTab(JUnitTestGURLs.BLUE_1, "Blue 1", 1705003000000L, 101);
    // This one is stale.
    private static final ForeignSessionTab TAB2 =
            new ForeignSessionTab(JUnitTestGURLs.GOOGLE_URL_DOG, "Google Dog", 1704999999999L, 102);
    private static final ForeignSessionTab TAB3 =
            new ForeignSessionTab(JUnitTestGURLs.CHROME_ABOUT, "About", 1705007000000L, 103);
    private static final ForeignSessionTab TAB4 =
            new ForeignSessionTab(JUnitTestGURLs.URL_1, "One", 1705000500000L, 104);
    private static final ForeignSessionTab TAB5 =
            new ForeignSessionTab(JUnitTestGURLs.MAPS_URL, "Maps", 1705004000000L, 105);
    // This one is the most recent.
    private static final ForeignSessionTab TAB6 =
            new ForeignSessionTab(JUnitTestGURLs.INITIAL_URL, "Initial", 1705008000000L, 106);
    private static final ForeignSessionTab TAB7 =
            new ForeignSessionTab(JUnitTestGURLs.HTTP_URL, "Old HTTP", 1705003000000L, 107);

    @Mock private SigninManager mSigninManager;
    @Mock private IdentityManager mIdentityManager;
    @Mock private SyncService mSyncService;
    @Mock private ForeignSessionHelper mForeignSessionHelper;

    @Captor private ArgumentCaptor<SignInStateObserver> mSignInStateObserverCaptor;
    @Captor private ArgumentCaptor<SyncStateChangedListener> mSyncStateChangedListenerCaptor;

    private ForeignSessionTabResumptionDataProvider mDataProvider;

    private int mStatusChangedCallbackCounter;
    private int mSuggestionCallbackCounter;
    private ArrayList<ForeignSession> mForeignSessions;

    @Before
    public void setUp() {
        mSigninManager = Mockito.mock(SigninManager.class);
        mIdentityManager = Mockito.mock(IdentityManager.class);
        mSyncService = Mockito.mock(SyncService.class);
        mForeignSessionHelper = Mockito.mock(ForeignSessionHelper.class);
        mSignInStateObserverCaptor = ArgumentCaptor.forClass(SignInStateObserver.class);
        mSyncStateChangedListenerCaptor = ArgumentCaptor.forClass(SyncStateChangedListener.class);

        // Populate `mForeignSessionHelper` with test data.
        ForeignSessionWindow desktopWindow1 =
                new ForeignSessionWindow(
                        /* timestamp= */ 1705009000000L,
                        /* sessionId= */ 201,
                        /* tabs= */ new ArrayList<>(Arrays.asList(TAB1, TAB2)));
        ForeignSessionWindow desktopWindow2 =
                new ForeignSessionWindow(
                        /* timestamp= */ 1705008000000L,
                        /* sessionId= */ 202,
                        /* tabs= */ new ArrayList<>(Arrays.asList(TAB3, TAB4)));
        ForeignSession desktopForeignSession =
                new ForeignSession(
                        /* tag= */ "TagForDesktop",
                        /* name= */ "My Desktop",
                        /* modifiedTime= */ 1705008900000L,
                        /* windows= */ new ArrayList<>(
                                Arrays.asList(desktopWindow1, desktopWindow2)),
                        /* formFactor= */ FormFactor.DESKTOP);

        ForeignSessionWindow tabletWindow1 =
                new ForeignSessionWindow(
                        /* timestamp= */ 1705009000000L,
                        /* sessionId= */ 301,
                        /* tabs= */ new ArrayList<>(Arrays.asList(TAB5, TAB6, TAB7)));
        ForeignSession tabletForeignSession =
                new ForeignSession(
                        /* tag= */ "TagForTablet",
                        /* name= */ "My Tablet",
                        /* modifiedTime= */ 1705009001000L,
                        /* windows= */ new ArrayList<>(Arrays.asList(tabletWindow1)),
                        /* formFactor= */ FormFactor.TABLET);

        ArrayList<ForeignSession> foreignSessions =
                new ArrayList<>(Arrays.asList(desktopForeignSession, tabletForeignSession));
        when(mForeignSessionHelper.getForeignSessions()).thenReturn(foreignSessions);
    }

    @After
    public void tearDown() {
        if (mDataProvider != null) {
            mDataProvider.destroy();
        }
    }

    @Test
    @SmallTest
    public void testMainFlow() {
        // Initially signed in and synced.
        createDataProvider(/* isSignedIn= */ true, /* isSynced= */ true);
        mDataProvider.fetchSuggestions(this::expectFilteredSortedSuggestions);
        // In production, fetchSuggestions() is async. In test, the provided fakes are eager. For
        // simplicity, in test we assume eagerness, and not worry about awaiting async calls.
        Assert.assertEquals(0, mStatusChangedCallbackCounter);
        Assert.assertEquals(1, mSuggestionCallbackCounter);
        // Repeat.
        mDataProvider.fetchSuggestions(this::expectFilteredSortedSuggestions);
        Assert.assertEquals(0, mStatusChangedCallbackCounter);
        Assert.assertEquals(2, mSuggestionCallbackCounter);
        // Disable sync.
        toggleIsSyncedThenNotify(false);
        Assert.assertEquals(1, mStatusChangedCallbackCounter);
        mDataProvider.fetchSuggestions(this::expectNullSuggestions);
        Assert.assertEquals(1, mStatusChangedCallbackCounter);
        Assert.assertEquals(3, mSuggestionCallbackCounter);
        // Enable sync.
        toggleIsSyncedThenNotify(true);
        Assert.assertEquals(2, mStatusChangedCallbackCounter);
        mDataProvider.fetchSuggestions(this::expectFilteredSortedSuggestions);
        Assert.assertEquals(2, mStatusChangedCallbackCounter);
        Assert.assertEquals(4, mSuggestionCallbackCounter);
        // Sign out.
        toggleIsSignedInThenNotify(false);
        Assert.assertEquals(3, mStatusChangedCallbackCounter);
        mDataProvider.fetchSuggestions(this::expectNullSuggestions);
        Assert.assertEquals(3, mStatusChangedCallbackCounter);
        Assert.assertEquals(5, mSuggestionCallbackCounter);
        // Sign in.
        toggleIsSignedInThenNotify(true);
        Assert.assertEquals(4, mStatusChangedCallbackCounter);
        mDataProvider.fetchSuggestions(this::expectFilteredSortedSuggestions);
        Assert.assertEquals(4, mStatusChangedCallbackCounter);
        Assert.assertEquals(6, mSuggestionCallbackCounter);
    }

    @Test
    @SmallTest
    public void testInitiallyNotSignedIn() {
        // Initially not signed in.
        createDataProvider(/* isSignedIn= */ false, /* isSynced= */ false);
        mDataProvider.fetchSuggestions(this::expectNullSuggestions);
        Assert.assertEquals(0, mStatusChangedCallbackCounter);
        Assert.assertEquals(1, mSuggestionCallbackCounter);
        // Sign in: Still bad.
        toggleIsSignedInThenNotify(true);
        Assert.assertEquals(1, mStatusChangedCallbackCounter);
        mDataProvider.fetchSuggestions(this::expectNullSuggestions);
        Assert.assertEquals(1, mStatusChangedCallbackCounter);
        Assert.assertEquals(2, mSuggestionCallbackCounter);
        // Enable sync: Finally good.
        toggleIsSyncedThenNotify(true);
        Assert.assertEquals(2, mStatusChangedCallbackCounter);
        mDataProvider.fetchSuggestions(this::expectFilteredSortedSuggestions);
        Assert.assertEquals(2, mStatusChangedCallbackCounter);
        Assert.assertEquals(3, mSuggestionCallbackCounter);
    }

    @Test
    @SmallTest
    public void testInitiallyNotSynced() {
        // Initially signed in, but not synced.
        createDataProvider(/* isSignedIn= */ true, /* isSynced= */ false);
        mDataProvider.fetchSuggestions(this::expectNullSuggestions);
        Assert.assertEquals(0, mStatusChangedCallbackCounter);
        Assert.assertEquals(1, mSuggestionCallbackCounter);
        // Enable sync: Now good.
        toggleIsSyncedThenNotify(true);
        Assert.assertEquals(1, mStatusChangedCallbackCounter);
        mDataProvider.fetchSuggestions(this::expectFilteredSortedSuggestions);
        Assert.assertEquals(1, mStatusChangedCallbackCounter);
        Assert.assertEquals(2, mSuggestionCallbackCounter);
    }

    private void createDataProvider(boolean isSignedIn, boolean isSynced) {
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(isSignedIn);
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(isSynced);
        mDataProvider =
                new ForeignSessionTabResumptionDataProvider(
                        /* signinManager= */ mSigninManager,
                        /* identityManager= */ mIdentityManager,
                        /* syncService= */ mSyncService,
                        /* foreignSessionHelper= */ mForeignSessionHelper,
                        /* forcedCurrentTimeMs= */ CURRENT_TIME_MS);

        verify(mSigninManager).addSignInStateObserver(mSignInStateObserverCaptor.capture());
        verify(mSyncService).addSyncStateChangedListener(mSyncStateChangedListenerCaptor.capture());
        Assert.assertEquals(mDataProvider, mSignInStateObserverCaptor.getValue());
        Assert.assertEquals(mDataProvider, mSyncStateChangedListenerCaptor.getValue());

        mDataProvider.setStatusChangedCallback(
                () -> {
                    ++mStatusChangedCallbackCounter;
                });
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

    private void expectFilteredSortedSuggestions(@Nullable List<SuggestionEntry> suggestions) {
        // There are 7 tabs total, but TAB3 is invalid, and TAB2 is stale, resulting in 5.
        ++mSuggestionCallbackCounter;
        Assert.assertEquals(5, suggestions.size());
        // Ensure sorted.
        SuggestionEntry prevEntry = null;
        for (SuggestionEntry entry : suggestions) {
            if (prevEntry != null) {
                Assert.assertTrue(prevEntry.compareTo(entry) < 0);
            }
            prevEntry = entry;
        }
        // Just check values for the first entry, which should be TAB6, being the most recent.
        SuggestionEntry firstEntry = suggestions.get(0);
        Assert.assertEquals("My Tablet", firstEntry.sourceName);
        Assert.assertEquals(JUnitTestGURLs.INITIAL_URL, firstEntry.url);
        Assert.assertEquals("Initial", firstEntry.title);
        Assert.assertEquals(1705008000000L, firstEntry.timestamp);
        Assert.assertEquals(106, firstEntry.id);
    }

    private void expectNullSuggestions(@Nullable List<SuggestionEntry> suggestions) {
        ++mSuggestionCallbackCounter;
        Assert.assertEquals(null, suggestions);
    }
}
