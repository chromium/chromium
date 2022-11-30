// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import org.junit.rules.ExternalResource;

/** Test rule to set clock WebappDataStorage uses for getting the current time. */
class MockWebappDataStorageClockRule extends ExternalResource {
    private static class MockClock extends WebappDataStorage.Clock {
        /**
         * Not zero so that callers of {@link currentTimeMillis()} substracting from the current
         * time get reasonable behavior.
         */
        private long mCurrentTime = 10000000000L;

        public void advance(long millis) {
            mCurrentTime += millis;
        }

        @Override
        public long currentTimeMillis() {
            return mCurrentTime;
        }
    }

    private MockClock mClock = new MockClock();

    public void advance(long millis) {
        mClock.advance(millis);
    }

    public long currentTimeMillis() {
        return mClock.currentTimeMillis();
    }

    @Override
    protected void before() throws Throwable {
        WebappDataStorage.setClockForTests(mClock);
    }

    @Override
    protected void after() {
        WebappDataStorage.setClockForTests(new WebappDataStorage.Clock());
    }
}
