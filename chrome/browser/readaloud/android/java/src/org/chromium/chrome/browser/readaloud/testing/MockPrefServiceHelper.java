// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.testing;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;

import org.mockito.invocation.InvocationOnMock;

import org.chromium.components.prefs.PrefService;

import java.util.HashMap;

// Provides a mock PrefService backed by a HashMap to make hasPrefPath(), get*(), and set*() work
// like the real PrefService.
public class MockPrefServiceHelper {
    private final PrefService mPrefService;
    private final HashMap<String, Object> mStorage;

    public MockPrefServiceHelper() {
        mPrefService = mock(PrefService.class);
        doAnswer(this::contains).when(mPrefService).hasPrefPath(any());
        doAnswer(invocation -> (String) get(invocation)).when(mPrefService).getString(any());
        doAnswer(invocation -> (Boolean) get(invocation)).when(mPrefService).getBoolean(any());
        doAnswer(this::set).when(mPrefService).setString(any(), any());
        doAnswer(this::set).when(mPrefService).setBoolean(any(), anyBoolean());
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
