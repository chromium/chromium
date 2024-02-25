// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.testing;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyDouble;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;

import org.mockito.invocation.InvocationOnMock;

import org.chromium.chrome.browser.readaloud.ReadAloudPrefs;
import org.chromium.components.prefs.PrefService;

import java.util.HashMap;
import java.util.Map;

// Provides a mock PrefService backed by a HashMap to make hasPrefPath(), get*(), and set*() work
// like the real PrefService.
public class MockPrefServiceHelper {
    private final PrefService mPrefService;
    private final HashMap<String, Object> mStorage;

    /**
     * Helper for mocking voice settings which depend on a native call.
     *
     * @param mockNatives Mock ReadAloudPrefs.Natives.
     * @param voices Voice settings map to be output from {@link ReadAloudPrefs.getVoices()}.
     */
    public static void setVoices(ReadAloudPrefs.Natives mockNatives, Map<String, String> voices) {
        doAnswer(
                        invocation -> {
                            ((Map<String, String>) invocation.getArguments()[1]).putAll(voices);
                            return null;
                        })
                .when(mockNatives)
                .getVoices(any(), any());
    }

    public MockPrefServiceHelper() {
        mPrefService = mock(PrefService.class);
        doAnswer(this::contains).when(mPrefService).hasPrefPath(any());
        doAnswer(invocation -> (String) get(invocation)).when(mPrefService).getString(any());
        doAnswer(invocation -> (Boolean) get(invocation)).when(mPrefService).getBoolean(any());
        doAnswer(invocation -> (Double) get(invocation)).when(mPrefService).getDouble(any());
        doAnswer(this::set).when(mPrefService).setString(any(), any());
        doAnswer(this::set).when(mPrefService).setBoolean(any(), anyBoolean());
        doAnswer(this::set).when(mPrefService).setDouble(any(), anyDouble());
        mStorage = new HashMap<>();
    }

    public PrefService getPrefService() {
        return mPrefService;
    }

    private boolean contains(InvocationOnMock inv) {
        return mStorage.containsKey(inv.getArguments()[0]);
    }

    private Object get(InvocationOnMock inv) {
        return mStorage.get(inv.getArguments()[0]);
    }

    private Object set(InvocationOnMock inv) {
        Object[] args = inv.getArguments();
        mStorage.put((String) args[0], args[1]);
        return null;
    }
}
