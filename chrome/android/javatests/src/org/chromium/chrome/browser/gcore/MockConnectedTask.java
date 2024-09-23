// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gcore;

import static org.junit.Assert.assertEquals;

/** Spying mock for ConnectedTask. */
class MockConnectedTask<T extends ChromeGoogleApiClient> extends ConnectedTask<T> {
    private int mDoWhenConnectedCount;
    private int mCleanUpCount;
    private int mRescheduleCount;

    public MockConnectedTask(T client) {
        super(client);
    }

    @Override
    protected final String getName() {
        return "MockConnectedTask";
    }

    @Override
    protected final void doWhenConnected(T client) {
        mDoWhenConnectedCount++;
    }

    @Override
    protected final void cleanUp() {
        mCleanUpCount++;
    }

    @Override
    protected final void retry(Runnable task, long delayMs) {
        mRescheduleCount++;
    }

    public void assertDoWhenConnectedCalled(int times) {
        assertEquals(times, mDoWhenConnectedCount);
        mDoWhenConnectedCount = 0;
    }

    public void assertCleanUpCalled(int times) {
        assertEquals(times, mCleanUpCount);
        mCleanUpCount = 0;
    }

    public void assertRescheduleCalled(int times) {
        assertEquals(times, mRescheduleCount);
        mRescheduleCount = 0;
    }

    public void assertNoOtherMethodsCalled() {
        assertEquals(0, mDoWhenConnectedCount + mCleanUpCount + mRescheduleCount);
    }
}
