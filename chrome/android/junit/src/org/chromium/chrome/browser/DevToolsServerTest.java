// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;

/** Unit tests for {@link DevToolsServer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DevToolsServerTest {
    private static final String SOCKET_PREFIX = "test_devtools_remote";
    private static final long NATIVE_DEV_TOOLS_SERVER = 12345L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DevToolsServer.Natives mMockDevToolsServerJni;
    @Mock private PrefService mPrefService;
    @Mock private PrefChangeRegistrar mPrefChangeRegistrar;
    @Captor private ArgumentCaptor<PrefChangeRegistrar.PrefObserver> mObserverCaptor;

    private DevToolsServer mDevToolsServer;

    @Before
    public void setUp() {
        DevToolsServerJni.setInstanceForTesting(mMockDevToolsServerJni);
        when(mMockDevToolsServerJni.initRemoteDebugging(SOCKET_PREFIX))
                .thenReturn(NATIVE_DEV_TOOLS_SERVER);

        mDevToolsServer = new DevToolsServer(SOCKET_PREFIX, mPrefService, mPrefChangeRegistrar);
        verify(mMockDevToolsServerJni).initRemoteDebugging(SOCKET_PREFIX);
        verify(mPrefChangeRegistrar)
                .addObserver(
                        eq(Pref.DEV_TOOLS_REMOTE_DEBUGGING_ALLOWED), mObserverCaptor.capture());
    }

    @Test
    public void testSetRemoteDebuggingEnabled_PrefAllowed_DefaultSecurity() {
        when(mPrefService.getBoolean(Pref.DEV_TOOLS_REMOTE_DEBUGGING_ALLOWED)).thenReturn(true);

        mDevToolsServer.setRemoteDebuggingEnabled(true, DevToolsServer.Security.DEFAULT);

        verify(mMockDevToolsServerJni)
                .setRemoteDebuggingEnabled(NATIVE_DEV_TOOLS_SERVER, true, false);
    }

    @Test
    public void testSetRemoteDebuggingEnabled_PrefAllowed_AllowDebugPermission() {
        when(mPrefService.getBoolean(Pref.DEV_TOOLS_REMOTE_DEBUGGING_ALLOWED)).thenReturn(true);

        mDevToolsServer.setRemoteDebuggingEnabled(
                true, DevToolsServer.Security.ALLOW_DEBUG_PERMISSION);

        verify(mMockDevToolsServerJni)
                .setRemoteDebuggingEnabled(NATIVE_DEV_TOOLS_SERVER, true, true);
    }

    @Test
    public void testSetRemoteDebuggingEnabled_DisabledByCaller() {
        mDevToolsServer.setRemoteDebuggingEnabled(
                false, DevToolsServer.Security.ALLOW_DEBUG_PERMISSION);

        verify(mMockDevToolsServerJni)
                .setRemoteDebuggingEnabled(NATIVE_DEV_TOOLS_SERVER, false, true);
        verify(mPrefService, never()).getBoolean(Pref.DEV_TOOLS_REMOTE_DEBUGGING_ALLOWED);
    }

    @Test
    public void testSetRemoteDebuggingEnabled_PrefDisallowed() {
        when(mPrefService.getBoolean(Pref.DEV_TOOLS_REMOTE_DEBUGGING_ALLOWED)).thenReturn(false);

        mDevToolsServer.setRemoteDebuggingEnabled(
                true, DevToolsServer.Security.ALLOW_DEBUG_PERMISSION);

        verify(mMockDevToolsServerJni)
                .setRemoteDebuggingEnabled(NATIVE_DEV_TOOLS_SERVER, false, true);
    }

    @Test
    public void testPreferenceChangeObserver() {
        when(mPrefService.getBoolean(Pref.DEV_TOOLS_REMOTE_DEBUGGING_ALLOWED)).thenReturn(true);
        mDevToolsServer.setRemoteDebuggingEnabled(
                true, DevToolsServer.Security.ALLOW_DEBUG_PERMISSION);
        verify(mMockDevToolsServerJni)
                .setRemoteDebuggingEnabled(NATIVE_DEV_TOOLS_SERVER, true, true);

        // Disallow remote debugging via pref
        when(mPrefService.getBoolean(Pref.DEV_TOOLS_REMOTE_DEBUGGING_ALLOWED)).thenReturn(false);
        mObserverCaptor.getValue().onPreferenceChange();

        verify(mMockDevToolsServerJni)
                .setRemoteDebuggingEnabled(NATIVE_DEV_TOOLS_SERVER, false, true);
    }

    @Test
    public void testDestroy() {
        mDevToolsServer.destroy();

        verify(mPrefChangeRegistrar).destroy();
        verify(mMockDevToolsServerJni).destroyRemoteDebugging(NATIVE_DEV_TOOLS_SERVER);

        // After destroy, updates should not call native methods
        mDevToolsServer.setRemoteDebuggingEnabled(
                true, DevToolsServer.Security.ALLOW_DEBUG_PERMISSION);
        verify(mMockDevToolsServerJni, never())
                .setRemoteDebuggingEnabled(anyLong(), anyBoolean(), anyBoolean());
    }
}
