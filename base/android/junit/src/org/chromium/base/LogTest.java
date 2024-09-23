// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit tests for {@link Log}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogTest {
    @Test
    public void normalizeTagTest() {
        assertEquals("cr_foo", Log.normalizeTag("foo"));
    }

    /** Tests that exceptions provided to the log functions are properly recognized and printed. */
    @Test
    public void exceptionLoggingTest() {
        Throwable t =
                new Throwable() {
                    @Override
                    public String toString() {
                        return "MyThrowable";
                    }
                };

        Throwable t2 =
                new Throwable() {
                    @Override
                    public String toString() {
                        return "MyOtherThrowable";
                    }
                };

        List<ShadowLog.LogItem> logs;

        // The throwable gets printed out
        Log.i("Foo", "Bar", t);
        logs = ShadowLog.getLogs();
        assertEquals(t, logs.get(logs.size() - 1).throwable);
        assertEquals("Bar", logs.get(logs.size() - 1).msg);

        // messageTemplate include %xx, print out normally.
        Log.i("Foo", "search?q=%E6%B5%8B%E8%AF%95", t);
        logs = ShadowLog.getLogs();
        assertEquals(t, logs.get(logs.size() - 1).throwable);
        assertEquals("search?q=%E6%B5%8B%E8%AF%95", logs.get(logs.size() - 1).msg);

        // Non throwable are properly identified
        Log.i("Foo", "Bar %s", t, "Baz");
        logs = ShadowLog.getLogs();
        assertNull(logs.get(logs.size() - 1).throwable);
        assertEquals("Bar MyThrowable", logs.get(logs.size() - 1).msg);

        // The last throwable is the one used that is going to be printed out
        Log.i("Foo", "Bar %s", t, t2);
        logs = ShadowLog.getLogs();
        assertEquals(t2, logs.get(logs.size() - 1).throwable);
        assertEquals("Bar MyThrowable", logs.get(logs.size() - 1).msg);
    }
}
