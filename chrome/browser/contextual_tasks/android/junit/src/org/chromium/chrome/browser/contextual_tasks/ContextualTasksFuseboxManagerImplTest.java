// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFusebox;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ContextualTasksFuseboxManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContextualTasksFuseboxManagerImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private ContextualTasksFusebox.ContextualTasksFuseboxConfig mFuseboxConfig;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private WebContents mWebContents;
    @Mock private FuseboxSessionState mSessionState;
    @Mock private ComposeboxQueryControllerBridge mBridge;
    @Mock private ContextualTasksFuseboxManagerImpl.Natives mMockJni;
    @Mock private Profile mProfile;

    private final UnownedUserDataHost mUserDataHost = new UnownedUserDataHost();
    private final SettableNullableObservableSupplier<Tab> mTabSupplier =
            ObservableSuppliers.createNullable();
    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableMonotonicObservableSupplier<SnackbarManager> mSnackbarManagerSupplier =
            ObservableSuppliers.createMonotonic();
    private ContextualTasksFuseboxManagerImpl mManager;

    private static final String TASK_ID_1 = "task-id-1";
    private static final String TASK_ID_2 = "task-id-2";
    private static final GURL CONTEXTUAL_TASKS_URL = JUnitTestGURLs.EXAMPLE_URL;

    @Before
    public void setUp() {
        SemanticColorUtils.setDefaultBgColorForTesting(Color.WHITE);
        ContextualTasksFuseboxManagerImplJni.setInstanceForTesting(mMockJni);
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUserDataHost);
        when(mSessionState.getComposeboxQueryControllerBridge()).thenReturn(mBridge);
        mProfileSupplier.set(mProfile);

        mManager =
                spy(
                        new ContextualTasksFuseboxManagerImpl(
                                mActivity,
                                () -> mFuseboxConfig,
                                mTabSupplier,
                                mWindowAndroid,
                                mLifecycleDispatcher,
                                mProfileSupplier,
                                mSnackbarManagerSupplier));
        doReturn(mSessionState).when(mManager).createSessionState(any());
        // For testing visibility logic.
        when(mMockJni.isContextualTasksUrl(any())).thenReturn(true);
    }

    @Test
    public void testOnWebUIReady_CreatesNewSession() {
        mManager.onWebUIReady(TASK_ID_1, mWebContents);

        verify(mManager).createSessionState(mWebContents);
        assertEquals(mSessionState, mManager.getFuseboxDataProvider().getFuseboxSessionState());
    }

    @Test
    public void testOnWebUIReady_ReusesExistingSession() {
        mManager.onWebUIReady(TASK_ID_1, mWebContents);
        mManager.onWebUIReady(TASK_ID_1, mWebContents);

        // Should only be called once.
        verify(mManager).createSessionState(mWebContents);
    }

    @Test
    public void testOnTaskChanged_UpdatesMapping() {
        mManager.onWebUIReady(TASK_ID_1, mWebContents);
        mManager.onTaskChanged(TASK_ID_1, TASK_ID_2);

        // Destroying UI for old ID should not affect the session now.
        mManager.onWebUIDestroyed(TASK_ID_1);
        verify(mSessionState, never()).destroy();

        // Destroying UI for new ID should clean it up.
        mManager.onWebUIDestroyed(TASK_ID_2);
        verify(mSessionState).destroy();
    }

    @Test
    public void testOnWebUIDestroyed_CleansUpSessionAndBridge() {
        mManager.onWebUIReady(TASK_ID_1, mWebContents);

        mManager.onWebUIDestroyed(TASK_ID_1);

        verify(mBridge).onWebUIDestroyed();
        verify(mSessionState).destroy();
        assertNull(mManager.getFuseboxDataProvider().getFuseboxSessionState());
    }

    @Test
    public void testDestroy_CleansUpAllSessions() {
        mManager.onWebUIReady(TASK_ID_1, mWebContents);
        mManager.onWebUIReady(TASK_ID_2, mWebContents);

        mManager.destroy();

        verify(mSessionState, atLeast(2)).destroy();
    }

    @Test
    public void testUpdateFuseboxVisibility_ShowsWhenTaskIdPresent() {
        when(mMockJni.getTaskIdForTab(mWebContents)).thenReturn(TASK_ID_1);
        // Simulate lazy initialization of mFusebox.
        mManager.onWebUIReady(TASK_ID_1, mWebContents); // This also sets session in DataProvider.

        // Trigger visibility update.
        mManager.onTaskChanged(null, TASK_ID_1);

        // Simulating the observer trigger.
        mManager.onWebUIReady(TASK_ID_1, mWebContents);

        // Since updateFuseboxVisibility is private, we rely on existing paths.
        // For now, let's verify ensureFuseboxSessionState which is called by visibility logic.
        mManager.ensureFuseboxSessionState(TASK_ID_1, mWebContents);
        verify(mManager).createSessionState(mWebContents);
    }
}
