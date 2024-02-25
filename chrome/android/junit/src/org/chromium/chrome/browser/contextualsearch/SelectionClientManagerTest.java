// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.junit.Assert.assertEquals;

import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.SelectAroundCaretResult;
import org.chromium.content_public.browser.SelectionClient;

/** Unit tests for the {@link SelectionClientManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectionClientManagerTest {
    // The client for most tests.
    private SelectionClientManager mManager;

    // The Contextual Search Client, which we always use when adding a client.
    private SelectionClientStub mContextualSearchClientStub;
    // The Smart Selection client, which we only use when constructing a manager.
    private SelectionClientStub mSmartSelectionClientStub;

    // Counters for calls to all of the above clients.
    int mCallsToAllClients;

    /** A SelectionClient for testing that tracks whether a method used by all clients was called. */
    private class SelectionClientStub implements SelectionClient {
        @Override
        public void onSelectionChanged(String selection) {
            mCallsToAllClients++;
        }

        @Override
        public boolean requestSelectionPopupUpdates(boolean shouldSuggest) {
            return true;
        }

        @Override
        public void onSelectionEvent(int eventType, float posXPix, float posYPix) {}

        @Override
        public void selectAroundCaretAck(@Nullable SelectAroundCaretResult result) {}

        @Override
        public void cancelAllRequests() {}
    }

    /** @return The number of calls to all clients. */
    int getCallsToAllClients() {
        return mCallsToAllClients;
    }

    /**
     * Makes a call that should apply to all clients and a call that should only apply to Smart
     * Selection clients.
     * @param manager The manager to call through.
     */
    private void makeClientCalls(SelectionClientManager manager) {
        SelectionClient optionalClient = manager.getSelectionClient();
        if (optionalClient == null) return;

        optionalClient.onSelectionChanged("unused");
        optionalClient.requestSelectionPopupUpdates(false);
    }

    @Before
    public void setUp() {
        // Assume Smart Selection is enabled and we have a Smart Selection client for most tests.
        mSmartSelectionClientStub = new SelectionClientStub();
        mManager = new SelectionClientManager(mSmartSelectionClientStub, true);
        // Create a ContextualSearch Client but don't use it for anything yet.
        mContextualSearchClientStub = new SelectionClientStub();
    }

    @Test
    @Feature({"TextInput", "SelectionClientManager"})
    public void testSmartSelectionClientOnly() {
        // Default setup has just the Smart Selection client.
        makeClientCalls(mManager);

        assertEquals(1, getCallsToAllClients());
    }

    @Test
    @Feature({"TextInput", "SelectionClientManager"})
    public void testSmartSelectionAndContextualSearchClients() {
        mManager.addContextualSearchSelectionClient(mContextualSearchClientStub);
        makeClientCalls(mManager);

        assertEquals(2, getCallsToAllClients());
    }

    @Test
    @Feature({"TextInput", "SelectionClientManager"})
    public void testContextualSearchClientOnly() {
        SelectionClientManager manager = new SelectionClientManager(null, false);
        manager.addContextualSearchSelectionClient(mContextualSearchClientStub);
        makeClientCalls(manager);

        assertEquals(1, getCallsToAllClients());
    }

    @Test
    @Feature({"TextInput", "SelectionClientManager"})
    public void testNoClients() {
        SelectionClientManager manager = new SelectionClientManager(null, false);
        makeClientCalls(manager);

        assertEquals(0, getCallsToAllClients());
    }

    @Test
    @Feature({"TextInput", "SelectionClientManager"})
    public void testNoClientsWithSmartSelectionEnabled() {
        SelectionClientManager manager = new SelectionClientManager(null, true);
        makeClientCalls(manager);

        assertEquals(0, getCallsToAllClients());
    }

    @Test(expected = AssertionError.class)
    @Feature({"TextInput", "SelectionClientManager"})
    public void testCantRemoveWithoutAdd() {
        mManager.removeContextualSearchSelectionClient();
    }

    @Test
    @Feature({"TextInput", "SelectionClientManager"})
    public void testRemoveAfterAddWorks() {
        mManager.addContextualSearchSelectionClient(mContextualSearchClientStub);
        mManager.removeContextualSearchSelectionClient();
        makeClientCalls(mManager);

        assertEquals(1, getCallsToAllClients());
    }

    @Test
    @Feature({"TextInput", "SelectionClientManager"})
    public void testMultipleAddAndRemove() {
        mManager.addContextualSearchSelectionClient(mContextualSearchClientStub);
        mManager.removeContextualSearchSelectionClient();
        mManager.addContextualSearchSelectionClient(mContextualSearchClientStub);
        makeClientCalls(mManager);

        assertEquals(2, getCallsToAllClients());
    }

    @Test(expected = AssertionError.class)
    @Feature({"TextInput", "SelectionClientManager"})
    public void testCantAddAgain() {
        SelectionClient someClient = new SelectionClientStub();
        mManager.addContextualSearchSelectionClient(someClient);
        mManager.addContextualSearchSelectionClient(someClient);
    }

    @Test(expected = AssertionError.class)
    @Feature({"TextInput", "SelectionClientManager"})
    public void testCantRemoveAgain() {
        SelectionClient someClient = new SelectionClientStub();
        mManager.addContextualSearchSelectionClient(someClient);
        mManager.removeContextualSearchSelectionClient();
        mManager.removeContextualSearchSelectionClient();
    }
}
