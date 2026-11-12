// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.junit.Assert.assertSame;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.jni_zero.JniRawPtr;
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
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.ui.base.WindowAndroid;

import java.util.Collections;

/** Tests for {@link ExclusiveAccessContext} and {@link ExclusiveAccessManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExclusiveAccessContextTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private ExclusiveAccessContext.Natives mExclusiveAccessContextJni;
    @Mock private ExclusiveAccessManager.Natives mExclusiveAccessManagerJni;
    private final UnownedUserDataHost mUnownedUserDataHost = new UnownedUserDataHost();
    private JniRawPtr<ExclusiveAccessContext.NativeExclusiveAccessContextAndroid> mNativePtr;

    @Before
    public void setUp() {
        ExclusiveAccessContextJni.setInstanceForTesting(mExclusiveAccessContextJni);
        ExclusiveAccessManagerJni.setInstanceForTesting(mExclusiveAccessManagerJni);
        mNativePtr = JniRawPtr.createForTesting(123L);
    }

    @After
    public void tearDown() {
        mNativePtr.release();
        SnackbarManagerProvider.detach(mSnackbarManager);
    }

    @Test
    public void testTouchDownNotifiesNative() {
        ActivityTabProvider activityTabProvider = new ActivityTabProvider();
        ExclusiveAccessContext context =
                ExclusiveAccessContext.create(
                        mNativePtr, mWindowAndroid, mFullscreenManager, activityTabProvider);

        // Simulate touch down on the active tab.
        context.mActiveTabObserver.onTouchDown();

        verify(mExclusiveAccessContextJni).onExclusiveAccessUserInput(mNativePtr);
    }

    @Test
    public void testGetsSnackbarManagerFromWindowAndroid() {
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUnownedUserDataHost);
        SnackbarManagerProvider.attach(mWindowAndroid, mSnackbarManager);
        ExclusiveAccessContext context =
                ExclusiveAccessContext.create(
                        mNativePtr,
                        mWindowAndroid,
                        mFullscreenManager,
                        new ActivityTabProvider());

        assertSame(mSnackbarManager, context.getSnackbarManager());
    }

    @Test
    public void testExitFullscreenModeForTab_withActiveTab() {
        Tab tab = mock(Tab.class);
        ExclusiveAccessContext context =
                ExclusiveAccessContext.create(
                        /* nativeExclusiveAccessContextAndroid= */ mNativePtr,
                        /* windowAndroid= */ mWindowAndroid,
                        /* fullscreenManager= */ mFullscreenManager,
                        /* activityTabProvider= */ new ActivityTabProvider());
        // Sets mActiveTab to a non-null Tab on ExclusiveAccessContext.
        context.forceActiveTab(tab);

        context.exitFullscreenModeForTab();

        verify(
                        mFullscreenManager,
                        description(
                                "Should exit fullscreen for the active tab when mActiveTab is"
                                        + " non-null."))
                .onExitFullscreen(tab);
        verify(
                        mFullscreenManager,
                        never().description(
                                        "Should not call exitPersistentFullscreenMode directly when"
                                                + " mActiveTab is non-null."))
                .exitPersistentFullscreenMode();
    }

    @Test
    public void testExitFullscreenModeForTab_withoutActiveTab() {
        ExclusiveAccessContext context =
                ExclusiveAccessContext.create(
                        /* nativeExclusiveAccessContextAndroid= */ mNativePtr,
                        /* windowAndroid= */ mWindowAndroid,
                        /* fullscreenManager= */ mFullscreenManager,
                        /* activityTabProvider= */ new ActivityTabProvider());

        context.exitFullscreenModeForTab();

        verify(
                        mFullscreenManager,
                        description(
                                "Should exit persistent fullscreen mode when mActiveTab is null."))
                .exitPersistentFullscreenMode();
        verify(
                        mFullscreenManager,
                        never().description(
                                        "Should not call onExitFullscreen(Tab) when mActiveTab is"
                                                + " null."))
                .onExitFullscreen(any());
    }

    @Test
    public void testToggleBrowserFullscreenMode() {
        when(mFullscreenManager.getPersistentFullscreenModeSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        JniUniquePtr<ExclusiveAccessManager.NativeExclusiveAccessManagerAndroid> nativePtr =
                JniUniquePtr.createForTesting(456L);
        when(mExclusiveAccessManagerJni.init(
                        /* caller= */ any(),
                        /* windowAndroid= */ any(),
                        /* fullscreenManager= */ any(),
                        /* activityTabProvider= */ any()))
                .thenReturn(nativePtr);

        TabModelSelector tabModelSelector = mock(TabModelSelector.class);
        when(tabModelSelector.getModels()).thenReturn(Collections.emptyList());

        ExclusiveAccessManager manager =
                new ExclusiveAccessManager(
                        /* fullscreenManager= */ mFullscreenManager,
                        /* desktopWindowStateManager= */ null);

        manager.initialize(
                /* modelSelector= */ tabModelSelector,
                /* windowAndroid= */ mWindowAndroid,
                /* activityTabProvider= */ new ActivityTabProvider());

        manager.toggleBrowserFullscreenMode(/* userInitiated= */ true);

        verify(
                        mExclusiveAccessManagerJni,
                        description(
                                "Should forward toggleBrowserFullscreenMode call to the JNI"
                                        + " bridge."))
                .toggleBrowserFullscreenMode(/* self= */ nativePtr, /* userInitiated= */ true);
        manager.destroy();
    }
}
