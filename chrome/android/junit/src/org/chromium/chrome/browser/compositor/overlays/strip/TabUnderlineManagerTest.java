// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.jni_zero.JniUniquePtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.contextual_tasks.ContextualTasksBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link TabUnderlineManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_TASKS_SIDE_PANEL)
public class TabUnderlineManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabUnderlineManager.Observer mObserver;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabUnderlineManager.Natives mMockJni;
    @Mock private ContextualTasksBridge mContextualTasksBridge;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private JniUniquePtr<TabUnderlineManager.NativeTabUnderlineManager> mNativePtr;

    private final UnownedUserDataHost mUserDataHost = new UnownedUserDataHost();
    private TabUnderlineManager mManager;

    private static final int TAB_ID_1 = 1;
    private static final int TAB_ID_2 = 2;
    private static final int TAB_ID_3 = 3;

    @Before
    public void setUp() {
        TabUnderlineManagerJni.setInstanceForTesting(mMockJni);
        when(mMockJni.init(any())).thenReturn(mNativePtr);
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUserDataHost);
        when(mTab1.getId()).thenReturn(TAB_ID_1);
        when(mTab2.getId()).thenReturn(TAB_ID_2);
        when(mTab3.getId()).thenReturn(TAB_ID_3);

        mManager = new TabUnderlineManager(mWindowAndroid);
        mManager.addObserver(mObserver);
    }

    @After
    public void tearDown() {
        TabUnderlineManagerJni.setInstanceForTesting(null);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CONTEXTUAL_TASKS_SIDE_PANEL)
    public void testRegisterAndUnregisterTab_contextualTasksDisabled() {
        verify(mMockJni).init(mManager);

        mManager.registerTab(mTab1);
        verify(mMockJni).registerTab(mNativePtr, mTab1);

        mManager.unregisterTab(TAB_ID_1);
        verify(mMockJni).unregisterTab(mNativePtr, TAB_ID_1);
    }

    @Test
    public void testRegisterTab_contextualTasksEnabled_deferredUntilBridgeReady() {
        verify(mMockJni).init(mManager);

        mManager.registerTab(mTab1);
        mManager.registerTab(mTab2);
        verify(mMockJni, never()).registerTab(any(), any());

        simulateContextualTasksBridgeReady();

        verify(mMockJni).registerTab(mNativePtr, mTab1);
        verify(mMockJni).registerTab(mNativePtr, mTab2);

        mManager.registerTab(mTab3);
        verify(mMockJni).registerTab(mNativePtr, mTab3);
    }

    @Test
    public void testUnregisterTab_contextualTasksEnabled_removesPending() {
        mManager.registerTab(mTab1);
        mManager.registerTab(mTab2);
        mManager.unregisterTab(TAB_ID_1);

        verify(mMockJni, never()).unregisterTab(any(), anyInt());

        simulateContextualTasksBridgeReady();

        verify(mMockJni).registerTab(mNativePtr, mTab2);
        verify(mMockJni, never()).registerTab(eq(mNativePtr), eq(mTab1));
    }

    @Test
    public void testSetUnderlineState() {
        mManager.setUnderlineState(TAB_ID_1, /* isUnderlined= */ true);
        verify(mObserver).onIndicatorStateChanged(TAB_ID_1, /* isActive= */ true);

        mManager.setUnderlineState(TAB_ID_1, /* isUnderlined= */ false);
        verify(mObserver).onIndicatorStateChanged(TAB_ID_1, /* isActive= */ false);
    }

    @Test
    public void testResetAnimationCycle() {
        mManager.resetAnimationCycle(TAB_ID_1);
        verify(mObserver).onResetAnimationCycle(TAB_ID_1);
    }

    @Test
    public void testAddAndRemoveObserver() {
        mManager.removeObserver(mObserver);
        mManager.setUnderlineState(TAB_ID_1, /* isUnderlined= */ true);
        verify(mObserver, never()).onIndicatorStateChanged(anyInt(), anyBoolean());
    }

    @Test
    public void testDestroy() {
        mManager.destroy();
        verify(mNativePtr).destroy();

        // Verify observers are cleared after destroy.
        mManager.setUnderlineState(TAB_ID_1, /* isUnderlined= */ true);
        verify(mObserver, never()).onIndicatorStateChanged(anyInt(), anyBoolean());

        // Verify double destroy is safe.
        mManager.destroy();
        verify(mNativePtr, times(1)).destroy();

        // Verify registering tab after destroy does nothing.
        clearInvocations(mMockJni);
        mManager.registerTab(mTab1);
        verify(mMockJni, never()).registerTab(any(), any());
    }

    private void simulateContextualTasksBridgeReady() {
        SettableMonotonicObservableSupplier<ContextualTasksBridge> supplier =
                (SettableMonotonicObservableSupplier<ContextualTasksBridge>)
                        ContextualTasksBridge.getSupplier(mWindowAndroid);
        supplier.set(mContextualTasksBridge);
    }
}
