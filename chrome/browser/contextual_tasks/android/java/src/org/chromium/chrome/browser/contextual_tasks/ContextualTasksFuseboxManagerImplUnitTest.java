// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;

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
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFusebox;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ContextualTasksFuseboxManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.CONTEXTUAL_TASKS_JAVA_FUSEBOX)
public class ContextualTasksFuseboxManagerImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private ContextualTasksFusebox.ContextualTasksFuseboxConfig mFuseboxConfig;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private WebContents mWebContents;
    @Mock private FuseboxSessionState mSessionState;
    @Mock private ComposeboxQueryControllerBridge mBridge;
    @Mock private ContextualTasksBridge.Natives mMockJni;
    @Mock private Profile mProfile;
    @Mock private ContextualTasksFusebox mFusebox;
    @Mock private View mFuseboxView;

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

    @Before
    public void setUp() {
        SemanticColorUtils.setDefaultBgColorForTesting(Color.WHITE);
        ContextualTasksBridgeJni.setInstanceForTesting(mMockJni);
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
        doReturn(mSessionState).when(mManager).createSessionState();
        // For testing visibility logic.
        when(mMockJni.isContextualTasksUrl(any())).thenReturn(true);
        when(mFusebox.getFuseboxView()).thenReturn(mFuseboxView);
        mManager.setFuseboxForTesting(mFusebox);
    }

    @Test
    public void testOnWebUIReady_CreatesNewSessionAndActivates() {
        mManager.onWebUIReady(TASK_ID_1, mWebContents);

        verify(mManager).createSessionState();
        verify(mSessionState).activate(any(), any(), any(), any());
        assertEquals(mSessionState, mManager.getFuseboxDataProvider().getFuseboxSessionState());
        assertEquals(mWebContents, mManager.getFuseboxDataProvider().getWebContents());
    }

    @Test
    public void testOnWebUIReady_ReusesExistingSession() {
        mManager.onWebUIReady(TASK_ID_1, mWebContents);
        mManager.onWebUIReady(TASK_ID_1, mWebContents);

        // Should only be called once.
        verify(mManager).createSessionState();
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
        assertNull(mManager.getFuseboxDataProvider().getWebContents());
    }

    @Test
    public void testDestroy_CleansUpAllSessions() {
        mManager.onWebUIReady(TASK_ID_1, mWebContents);
        mManager.onWebUIReady(TASK_ID_2, mWebContents);

        mManager.destroy();

        verify(mSessionState, atLeast(2)).destroy();
    }

    @Test
    public void testUpdateFuseboxVisibility_ReadyFirstLogic() {
        Tab tab = mock(Tab.class);
        WebContents tabWebContents = mock(WebContents.class);
        when(tab.getWebContents()).thenReturn(tabWebContents);
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mMockJni.getTaskIdForTab(tabWebContents)).thenReturn(TASK_ID_1);

        // 1. Initial navigation (WebUI not ready yet).
        mTabSupplier.set(tab);

        // Under Ready-First logic, we wait for onWebUIReady. No session should be created yet.
        verify(mManager, never()).createSessionState();
        assertNull(mManager.getFuseboxDataProvider().getFuseboxSessionState());

        // 2. WebUI becomes ready.
        mManager.onWebUIReady(TASK_ID_1, mWebContents);

        // Now session should be created and WebContents set.
        verify(mManager).createSessionState();
        assertEquals(mWebContents, mManager.getFuseboxDataProvider().getWebContents());

        // 3. Switch away to non-task tab.
        when(mMockJni.getTaskIdForTab(any())).thenReturn(null);
        mTabSupplier.set(mock(Tab.class));
        assertNull(mManager.getFuseboxDataProvider().getFuseboxSessionState());
        assertNull(mManager.getFuseboxDataProvider().getWebContents());

        // 4. Switch back to task tab.
        when(mMockJni.getTaskIdForTab(tabWebContents)).thenReturn(TASK_ID_1);
        when(mSessionState.getContextualTasksWebContents()).thenReturn(mWebContents);
        mTabSupplier.set(tab);

        // Should restore context.
        assertEquals(mSessionState, mManager.getFuseboxDataProvider().getFuseboxSessionState());
        assertEquals(mWebContents, mManager.getFuseboxDataProvider().getWebContents());
    }

    @Test
    public void testUpdateFuseboxVisibility_BrowsingTabWithTask() {
        Tab tab = mock(Tab.class);
        WebContents tabWebContents = mock(WebContents.class);
        when(tab.getWebContents()).thenReturn(tabWebContents);
        // Regular browsing URL (NOT contextual tasks).
        when(tab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mMockJni.getTaskIdForTab(tabWebContents)).thenReturn(TASK_ID_1);

        // 1. WebUI is ready for this task.
        mManager.onWebUIReady(TASK_ID_1, mWebContents);
        verify(mFuseboxView).setVisibility(View.VISIBLE);
        when(mSessionState.getContextualTasksWebContents()).thenReturn(mWebContents);

        // 2. Ensure next calls to isContextualTasksUrl return false.
        when(mMockJni.isContextualTasksUrl(any())).thenReturn(false);

        // 3. Switch to the browsing tab.
        mTabSupplier.set(tab);
        mManager.updateFuseboxVisibility(tab);

        // Plumbing should be ACTIVE.
        assertEquals(mSessionState, mManager.getFuseboxDataProvider().getFuseboxSessionState());
        assertEquals(mWebContents, mManager.getFuseboxDataProvider().getWebContents());

        // UI should be HIDDEN.
        verify(mFuseboxView).setVisibility(View.GONE);
        verify(mFusebox).endInput();
    }
}
