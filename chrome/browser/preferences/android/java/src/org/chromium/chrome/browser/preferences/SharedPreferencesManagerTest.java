// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Unit tests for {@link SharedPreferencesManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SharedPreferencesManagerTest {
    @Mock
    private ChromePreferenceKeyChecker mChecker;

    private static final KeyPrefix TEST_PREFIX = new KeyPrefix("TestPrefix.*");
    private static final String PREFIXED_KEY_1 = TEST_PREFIX.createKey("stemA");
    private static final String PREFIXED_KEY_2 = TEST_PREFIX.createKey("stemB");
    private static final String PREFIXED_KEY_3 = TEST_PREFIX.createKey(33);

    private SharedPreferencesManager mSubject;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mSubject = new SharedPreferencesManager(mChecker);
    }

    @Test
    @SmallTest
    public void testWriteReadInt() {
        // Verify default return values when no value is written.
        assertEquals(0, mSubject.readInt("int_key"));
        assertEquals(987, mSubject.readInt("int_key", 987));
        assertFalse(mSubject.contains("int_key"));

        // Write a value.
        mSubject.writeInt("int_key", 123);

        // Verify value written can be read.
        assertEquals(123, mSubject.readInt("int_key"));
        assertEquals(123, mSubject.readInt("int_key", 987));
        assertTrue(mSubject.contains("int_key"));

        // Remove the value.
        mSubject.removeKey("int_key");

        // Verify the removed value is not returned anymore.
        assertEquals(0, mSubject.readInt("int_key"));
        assertFalse(mSubject.contains("int_key"));
    }

    @Test
    @SmallTest
    public void testIncrementInt() {
        mSubject.writeInt("int_key", 100);
        int result = mSubject.incrementInt("int_key");

        assertEquals(101, result);
        assertEquals(101, mSubject.readInt("int_key"));
    }

    @Test
    @SmallTest
    public void testIncrementIntDefault() {
        int result = mSubject.incrementInt("int_key");

        assertEquals(1, result);
        assertEquals(1, mSubject.readInt("int_key"));
    }

    @Test
    @SmallTest
    public void testWriteReadBoolean() {
        // Verify default return values when no value is written.
        assertEquals(false, mSubject.readBoolean("bool_key", false));
        assertEquals(true, mSubject.readBoolean("bool_key", true));
        assertFalse(mSubject.contains("bool_key"));

        // Write a value.
        mSubject.writeBoolean("bool_key", true);

        // Verify value written can be read.
        assertEquals(true, mSubject.readBoolean("bool_key", false));
        assertEquals(true, mSubject.readBoolean("bool_key", true));
        assertTrue(mSubject.contains("bool_key"));

        // Remove the value.
        mSubject.removeKey("bool_key");

        // Verify the removed value is not returned anymore.
        assertEquals(false, mSubject.readBoolean("bool_key", false));
        assertEquals(true, mSubject.readBoolean("bool_key", true));
        assertFalse(mSubject.contains("bool_key"));
    }

    @Test
    @SmallTest
    public void testWriteReadString() {
        // Verify default return values when no value is written.
        assertEquals("default", mSubject.readString("string_key", "default"));
        assertFalse(mSubject.contains("string_key"));

        // Write a value.
        mSubject.writeString("string_key", "foo");

        // Verify value written can be read.
        assertEquals("foo", mSubject.readString("string_key", "default"));
        assertTrue(mSubject.contains("string_key"));

        // Remove the value.
        mSubject.removeKey("string_key");

        // Verify the removed value is not returned anymore.
        assertEquals("default", mSubject.readString("string_key", "default"));
        assertFalse(mSubject.contains("string_key"));
    }

    @Test
    @SmallTest
    public void testWriteReadLong() {
        // Verify default return values when no value is written.
        assertEquals(0, mSubject.readLong("long_key"));
        assertEquals(9876543210L, mSubject.readLong("long_key", 9876543210L));
        assertFalse(mSubject.contains("long_key"));

        // Write a value.
        mSubject.writeLong("long_key", 9999999999L);

        // Verify value written can be read.
        assertEquals(9999999999L, mSubject.readLong("long_key"));
        assertEquals(9999999999L, mSubject.readLong("long_key", 9876543210L));
        assertTrue(mSubject.contains("long_key"));

        // Remove the value.
        mSubject.removeKey("long_key");

        // Verify the removed value is not returned anymore.
        assertEquals(0, mSubject.readLong("long_key"));
        assertFalse(mSubject.contains("long_key"));
    }

    @Test
    @SmallTest
    public void testWriteReadFloat() {
        // Verify default return values when no value is written.
        assertEquals(1.5f, mSubject.readFloat("float_key", 1.5f), 0.001f);
        assertFalse(mSubject.contains("float_key"));

        // Write a value.
        mSubject.writeFloat("float_key", 42.42f);

        // Verify value written can be read.
        assertEquals(42.42f, mSubject.readFloat("float_key", 1.5f), 0.001f);
        assertTrue(mSubject.contains("float_key"));

        // Remove the value.
        mSubject.removeKey("float_key");

        // Verify the removed value is not returned anymore.
        assertEquals(1.5f, mSubject.readFloat("float_key", 1.5f), 0.001f);
        assertFalse(mSubject.contains("float_key"));
    }

    @Test
    @SmallTest
    public void testWriteReadDouble() {
        // Verify default return values when no value is written.
        assertEquals(1.5d, mSubject.readDouble("double_key", 1.5d), 0.001f);
        assertFalse(mSubject.contains("double_key"));

        // Write a value.
        mSubject.writeDouble("double_key", 42.42f);

        // Verify value written can be read.
        assertEquals(42.42d, mSubject.readDouble("double_key", 1.5d), 0.001f);
        assertTrue(mSubject.contains("double_key"));

        // Remove the value.
        mSubject.removeKey("double_key");

        // Verify the removed value is not returned anymore.
        assertEquals(1.5d, mSubject.readDouble("double_key", 1.5d), 0.001f);
        assertFalse(mSubject.contains("double_key"));
    }

    @Test
    @SmallTest
    public void testWriteReadStringSet() {
        Set<String> defaultStringSet = new HashSet<>(Arrays.asList("a", "b", "c"));
        Set<String> exampleStringSet = new HashSet<>(Arrays.asList("d", "e"));

        // Verify default return values when no value is written.
        assertEquals(Collections.emptySet(), mSubject.readStringSet("string_set_key"));
        assertEquals(defaultStringSet, mSubject.readStringSet("string_set_key", defaultStringSet));
        assertNull(mSubject.readStringSet("string_set_key", null));
        assertFalse(mSubject.contains("string_set_key"));

        // Write a value.
        mSubject.writeStringSet("string_set_key", exampleStringSet);

        // Verify value written can be read.
        assertEquals(exampleStringSet, mSubject.readStringSet("string_set_key"));
        assertEquals(exampleStringSet, mSubject.readStringSet("string_set_key", defaultStringSet));
        assertEquals(exampleStringSet, mSubject.readStringSet("string_set_key", null));
        assertTrue(mSubject.contains("string_set_key"));

        // Remove the value.
        mSubject.removeKey("string_set_key");

        // Verify the removed value is not returned anymore.
        assertEquals(Collections.emptySet(), mSubject.readStringSet("string_set_key"));
        assertFalse(mSubject.contains("string_set_key"));
    }

    @Test
    @SmallTest
    public void testAddToStringSet() {
        mSubject.writeStringSet("string_set_key", new HashSet<>(Collections.singletonList("bar")));
        mSubject.addToStringSet("string_set_key", "foo");

        assertEquals(new HashSet<>(Arrays.asList("foo", "bar")),
                mSubject.readStringSet("string_set_key"));
    }

    @Test
    @SmallTest
    public void testAddToStringSetDefault() {
        mSubject.addToStringSet("string_set_key", "foo");

        assertEquals(new HashSet<>(Collections.singletonList("foo")),
                mSubject.readStringSet("string_set_key"));
    }

    @Test
    @SmallTest
    public void testRemoveFromStringSet() {
        mSubject.writeStringSet("string_set_key", new HashSet<>(Arrays.asList("foo", "bar")));
        mSubject.removeFromStringSet("string_set_key", "foo");

        assertEquals(new HashSet<>(Collections.singletonList("bar")),
                mSubject.readStringSet("string_set_key"));
    }

    @Test
    @SmallTest
    public void testRemoveFromStringSetDefault() {
        mSubject.removeFromStringSet("string_set_key", "foo");

        assertEquals(Collections.emptySet(), mSubject.readStringSet("string_set_key"));
    }

    @Test(expected = UnsupportedOperationException.class)
    @SmallTest
    public void testReadStringSet_nonEmpty_returnsUnmodifiable() {
        Set<String> exampleStringSet = new HashSet<>(Arrays.asList("d", "e"));
        mSubject.writeStringSet("string_set_key", exampleStringSet);

        Set<String> unmodifiableSet = mSubject.readStringSet("string_set_key");

        // Should throw an exception
        unmodifiableSet.add("f");
    }

    @Test
    @SmallTest
    public void testWriteIntSync() {
        // Verify default return values when no value is written.
        assertEquals(0, mSubject.readInt("int_key"));

        // Write a value.
        boolean success = mSubject.writeIntSync("int_key", 123);

        // Verify value written can be read.
        assertEquals(123, mSubject.readInt("int_key"));
        assertTrue(success);
    }

    @Test
    @SmallTest
    public void testWriteBooleanSync() {
        // Verify default return values when no value is written.
        assertEquals(false, mSubject.readBoolean("bool_key", false));

        // Write a value.
        boolean success = mSubject.writeBooleanSync("bool_key", true);

        // Verify value written can be read.
        assertEquals(true, mSubject.readBoolean("bool_key", false));
        assertTrue(success);
    }

    @Test
    @SmallTest
    public void testWriteStringSync() {
        // Verify default return values when no value is written.
        assertEquals("default", mSubject.readString("string_key", "default"));

        // Write a value.
        boolean success = mSubject.writeStringSync("string_key", "foo");

        // Verify value written can be read.
        assertEquals("foo", mSubject.readString("string_key", "default"));
        assertTrue(success);
    }

    @Test
    @SmallTest
    public void testWriteLongSync() {
        // Verify default return values when no value is written.
        assertEquals(0, mSubject.readLong("long_key"));

        // Write a value.
        boolean success = mSubject.writeLongSync("long_key", 9999999999L);

        // Verify value written can be read.
        assertEquals(9999999999L, mSubject.readLong("long_key"));
        assertTrue(success);
    }

    @Test
    @SmallTest
    public void testWriteFloatSync() {
        // Verify default return values when no value is written.
        assertEquals(0f, mSubject.readFloat("float_key", 0f), 0f);

        // Write a value.
        boolean success = mSubject.writeFloatSync("float_key", 42.42f);

        // Verify value written can be read.
        assertEquals(42.42f, mSubject.readFloat("float_key", 1.5f), 0.001f);
        assertTrue(success);
    }

    @Test
    @SmallTest
    public void testRemoveKeySync() {
        // Write a value.
        mSubject.writeIntSync("int_key", 123);
        assertEquals(123, mSubject.readInt("int_key", 999));

        // Remove it
        boolean success = mSubject.removeKeySync("int_key");

        // Verify value was removed.
        assertEquals(999, mSubject.readInt("int_key", 999));
        assertTrue(success);
    }

    @Test
    @SmallTest
    public void testRemoveKeys() {
        KeyPrefix otherPrefix = new KeyPrefix("OtherPrefix.*");

        // Write some values, both prefixes and not prefixed.
        mSubject.writeInt(PREFIXED_KEY_1, 111);
        mSubject.writeInt(PREFIXED_KEY_2, 222);
        mSubject.writeInt(PREFIXED_KEY_3, 333);
        mSubject.writeInt(otherPrefix.createKey("stemA"), 444);
        mSubject.writeInt("OtherKey", 555);

        // Remove them
        mSubject.removeKeysWithPrefix(TEST_PREFIX);

        // Verify only values for the given prefix were removed.
        assertEquals(0, mSubject.readInt(PREFIXED_KEY_1, 0));
        assertEquals(0, mSubject.readInt(PREFIXED_KEY_2, 0));
        assertEquals(0, mSubject.readInt(PREFIXED_KEY_3, 0));
        assertEquals(444, mSubject.readInt(otherPrefix.createKey("stemA"), 0));
        assertEquals(555, mSubject.readInt("OtherKey", 0));
    }

    @Test
    @SmallTest
    public void testReadStringsWithPrefix() {
        // Write some values.
        mSubject.writeString(PREFIXED_KEY_1, "first");
        mSubject.writeString(PREFIXED_KEY_2, "second");
        mSubject.writeString(PREFIXED_KEY_3, "third");
        mSubject.writeString("OtherKey", "fourth");

        // Verify values written are read with readStringsWithPrefix().
        Map<String, String> result = mSubject.readStringsWithPrefix(TEST_PREFIX);
        assertEquals(3, result.size());

        assertEquals("first", result.get(PREFIXED_KEY_1));
        assertEquals("second", result.get(PREFIXED_KEY_2));
        assertEquals("third", result.get(PREFIXED_KEY_3));
    }

    @Test
    @SmallTest
    public void testReadIntsWithPrefix() {
        // Write some values.
        mSubject.writeInt(PREFIXED_KEY_1, 1);
        mSubject.writeInt(PREFIXED_KEY_2, 2);
        mSubject.writeInt(PREFIXED_KEY_3, 3);
        mSubject.writeInt("OtherKey", 4);

        // Verify values written are read with readIntsWithPrefix().
        Map<String, Integer> result = mSubject.readIntsWithPrefix(TEST_PREFIX);
        assertEquals(3, result.size());
        assertEquals(1, result.get(PREFIXED_KEY_1).intValue());
        assertEquals(2, result.get(PREFIXED_KEY_2).intValue());
        assertEquals(3, result.get(PREFIXED_KEY_3).intValue());
    }

    @Test
    @SmallTest
    public void testReadLongsWithPrefix() {
        // Write some values.
        mSubject.writeLong(PREFIXED_KEY_1, 21474836470001L);
        mSubject.writeLong(PREFIXED_KEY_2, 21474836470002L);
        mSubject.writeLong(PREFIXED_KEY_3, 21474836470003L);
        mSubject.writeLong("OtherKey", 21474836470004L);

        // Verify values written are read with readLongsWithPrefix().
        Map<String, Long> result = mSubject.readLongsWithPrefix(TEST_PREFIX);
        assertEquals(3, result.size());
        assertEquals(21474836470001L, result.get(PREFIXED_KEY_1).longValue());
        assertEquals(21474836470002L, result.get(PREFIXED_KEY_2).longValue());
        assertEquals(21474836470003L, result.get(PREFIXED_KEY_3).longValue());
    }

    @Test
    @SmallTest
    public void testReadFloatsWithPrefix() {
        // Write some values.
        mSubject.writeFloat(PREFIXED_KEY_1, 1.0f);
        mSubject.writeFloat(PREFIXED_KEY_2, 2.5f);
        mSubject.writeFloat(PREFIXED_KEY_3, 3.5f);
        mSubject.writeFloat("OtherKey", 4.0f);

        // Verify values written are read with readFloatsWithPrefix().
        Map<String, Float> result = mSubject.readFloatsWithPrefix(TEST_PREFIX);
        assertEquals(3, result.size());
        assertEquals(1.0f, result.get(PREFIXED_KEY_1), 1e-10);
        assertEquals(2.5f, result.get(PREFIXED_KEY_2), 1e-10);
        assertEquals(3.5f, result.get(PREFIXED_KEY_3), 1e-10);
    }

    @Test
    @SmallTest
    public void testReadDoublesWithPrefix() {
        // Write some values.
        mSubject.writeDouble(PREFIXED_KEY_1, 1.0);
        mSubject.writeDouble(PREFIXED_KEY_2, 2.5);
        mSubject.writeDouble(PREFIXED_KEY_3, 3.5);
        mSubject.writeDouble("OtherKey", 4.0);

        // Verify values written are read with readDoublesWithPrefix().
        Map<String, Double> result = mSubject.readDoublesWithPrefix(TEST_PREFIX);
        assertEquals(3, result.size());
        assertEquals(1.0, result.get(PREFIXED_KEY_1), 1e-10);
        assertEquals(2.5, result.get(PREFIXED_KEY_2), 1e-10);
        assertEquals(3.5, result.get(PREFIXED_KEY_3).doubleValue(), 1e-10);
    }

    @Test
    @SmallTest
    public void testReadBooleansWithPrefix() {
        // Write some values.
        mSubject.writeBoolean(PREFIXED_KEY_1, true);
        mSubject.writeBoolean(PREFIXED_KEY_2, false);
        mSubject.writeBoolean(PREFIXED_KEY_3, true);
        mSubject.writeBoolean("OtherKey", true);

        // Verify values written are read with readBooleansWithPrefix().
        Map<String, Boolean> result = mSubject.readBooleansWithPrefix(TEST_PREFIX);
        assertEquals(3, result.size());
        assertTrue(result.get(PREFIXED_KEY_1));
        assertFalse(result.get(PREFIXED_KEY_2));
        assertTrue(result.get(PREFIXED_KEY_3));
    }

    @Test
    @SmallTest
    public void testWriteStrings() {
        mSubject.writeStrings(new HashMap<String, String>() {
            {
                put(PREFIXED_KEY_1, "first");
                put("string_key", "second");
            }
        });

        // Verify values can be read
        assertEquals("first", mSubject.readString(PREFIXED_KEY_1, ""));
        assertEquals("second", mSubject.readString("string_key", ""));
    }

    @Test
    @SmallTest
    public void testWriteInts() {
        mSubject.writeInts(new HashMap<String, Integer>() {
            {
                put(PREFIXED_KEY_1, 1001);
                put("int_key", 1002);
            }
        });

        // Verify values can be read
        assertEquals(1001, mSubject.readInt(PREFIXED_KEY_1));
        assertEquals(1002, mSubject.readInt("int_key"));
    }

    @Test
    @SmallTest
    public void testWriteLongs() {
        mSubject.writeLongs(new HashMap<String, Long>() {
            {
                put(PREFIXED_KEY_1, 21474836470001L);
                put("long_key", 21474836470002L);
            }
        });

        // Verify values can be read
        assertEquals(21474836470001L, mSubject.readLong(PREFIXED_KEY_1));
        assertEquals(21474836470002L, mSubject.readLong("long_key"));
    }

    @Test
    @SmallTest
    public void testWriteFloats() {
        mSubject.writeFloats(new HashMap<String, Float>() {
            {
                put(PREFIXED_KEY_1, 1.5f);
                put("float_key", 2.5f);
            }
        });

        // Verify values can be read
        assertEquals(1.5f, mSubject.readFloat(PREFIXED_KEY_1, 0f), 1e-10);
        assertEquals(2.5f, mSubject.readFloat("float_key", 0f), 1e-10);
    }

    @Test
    @SmallTest
    public void testWriteDoubles() {
        mSubject.writeDoubles(new HashMap<String, Double>() {
            {
                put(PREFIXED_KEY_1, 1.5);
                put("double_key", 2.5);
            }
        });

        // Verify values can be read
        assertEquals(1.5, mSubject.readDouble(PREFIXED_KEY_1, 0.0), 1e-10);
        assertEquals(2.5, mSubject.readDouble("double_key", 0.0), 1e-10);
    }

    @Test
    @SmallTest
    public void testWriteBooleans() {
        mSubject.writeBooleans(new HashMap<String, Boolean>() {
            {
                put(PREFIXED_KEY_1, true);
                put("long_key", false);
            }
        });

        // Verify values can be read
        assertTrue(mSubject.readBoolean(PREFIXED_KEY_1, false));
        assertFalse(mSubject.readBoolean("long_key", true));
    }

    @Test
    @SmallTest
    public void testCheckerIsCalled() {
        mSubject.writeInt("int_key", 123);
        verify(mChecker, times(1)).checkIsKeyInUse("int_key");
        mSubject.readInt("int_key");
        verify(mChecker, times(2)).checkIsKeyInUse("int_key");
        mSubject.incrementInt("int_key");
        verify(mChecker, times(3)).checkIsKeyInUse("int_key");
        mSubject.writeInts(Collections.singletonMap("int_key", 123));
        verify(mChecker, times(4)).checkIsKeyInUse("int_key");

        mSubject.writeBoolean("bool_key", true);
        verify(mChecker, times(1)).checkIsKeyInUse("bool_key");
        mSubject.readBoolean("bool_key", false);
        verify(mChecker, times(2)).checkIsKeyInUse("bool_key");
        mSubject.writeBooleans(Collections.singletonMap("bool_key", true));
        verify(mChecker, times(3)).checkIsKeyInUse("bool_key");

        mSubject.writeString("string_key", "foo");
        verify(mChecker, times(1)).checkIsKeyInUse("string_key");
        mSubject.readString("string_key", "");
        verify(mChecker, times(2)).checkIsKeyInUse("string_key");
        mSubject.writeStrings(Collections.singletonMap("string_key", "foo"));
        verify(mChecker, times(3)).checkIsKeyInUse("string_key");

        mSubject.writeLong("long_key", 999L);
        verify(mChecker, times(1)).checkIsKeyInUse("long_key");
        mSubject.readLong("long_key");
        verify(mChecker, times(2)).checkIsKeyInUse("long_key");
        mSubject.writeLongs(Collections.singletonMap("long_key", 999L));
        verify(mChecker, times(3)).checkIsKeyInUse("long_key");

        mSubject.writeFloat("float_key", 2.5f);
        verify(mChecker, times(1)).checkIsKeyInUse("float_key");
        mSubject.readFloat("float_key", 0f);
        verify(mChecker, times(2)).checkIsKeyInUse("float_key");
        mSubject.writeFloats(Collections.singletonMap("float_key", 2.5f));
        verify(mChecker, times(3)).checkIsKeyInUse("float_key");

        mSubject.writeDouble("double_key", 2.5d);
        verify(mChecker, times(1)).checkIsKeyInUse("double_key");
        mSubject.readDouble("double_key", 0d);
        verify(mChecker, times(2)).checkIsKeyInUse("double_key");
        mSubject.writeDoubles(Collections.singletonMap("double_key", 2.5d));
        verify(mChecker, times(3)).checkIsKeyInUse("double_key");

        mSubject.writeStringSet("string_set_key", new HashSet<>());
        verify(mChecker, times(1)).checkIsKeyInUse("string_set_key");
        mSubject.readStringSet("string_set_key");
        verify(mChecker, times(2)).checkIsKeyInUse("string_set_key");
        mSubject.addToStringSet("string_set_key", "bar");
        verify(mChecker, times(3)).checkIsKeyInUse("string_set_key");
        mSubject.removeFromStringSet("string_set_key", "bar");
        verify(mChecker, times(4)).checkIsKeyInUse("string_set_key");

        mSubject.removeKey("some_key");
        verify(mChecker, times(1)).checkIsKeyInUse("some_key");
        mSubject.contains("some_key");
        verify(mChecker, times(2)).checkIsKeyInUse("some_key");

        mSubject.readBooleansWithPrefix(TEST_PREFIX);
        verify(mChecker, times(1)).checkIsPrefixInUse(TEST_PREFIX);
        mSubject.readIntsWithPrefix(TEST_PREFIX);
        verify(mChecker, times(2)).checkIsPrefixInUse(TEST_PREFIX);
        mSubject.readLongsWithPrefix(TEST_PREFIX);
        verify(mChecker, times(3)).checkIsPrefixInUse(TEST_PREFIX);
        mSubject.readFloatsWithPrefix(TEST_PREFIX);
        verify(mChecker, times(4)).checkIsPrefixInUse(TEST_PREFIX);
        mSubject.readDoublesWithPrefix(TEST_PREFIX);
        verify(mChecker, times(5)).checkIsPrefixInUse(TEST_PREFIX);
        mSubject.readStringsWithPrefix(TEST_PREFIX);
        verify(mChecker, times(6)).checkIsPrefixInUse(TEST_PREFIX);
        mSubject.removeKeysWithPrefix(TEST_PREFIX);
        verify(mChecker, times(7)).checkIsPrefixInUse(TEST_PREFIX);
    }

    private static class TestObserver implements SharedPreferencesManager.Observer {
        int mEventCount;

        @Override
        public void onPreferenceChanged(String key) {
            mEventCount++;
        }
    }

    @Test
    @SmallTest
    public void testObserver() {
        TestObserver observer = new TestObserver();
        mSubject.addObserver(observer);

        int expectedEventCount = 0;

        // Each write should issue an event.
        mSubject.writeInt("int_key", 99);
        expectedEventCount++;
        assertEquals(expectedEventCount, observer.mEventCount);

        mSubject.writeInt("int_key", 88);
        expectedEventCount++;
        assertEquals(expectedEventCount, observer.mEventCount);

        mSubject.incrementInt("int_key");
        expectedEventCount++;
        assertEquals(expectedEventCount, observer.mEventCount);

        mSubject.writeInts(Collections.singletonMap("int_key", 88));
        expectedEventCount++;
        assertEquals(expectedEventCount, observer.mEventCount);

        // Reads should not trigger events.
        mSubject.readInt("int_key");
        assertEquals(expectedEventCount, observer.mEventCount);

        // Removing a key should trigger event.
        mSubject.removeKey("int_key");
        expectedEventCount++;
        assertEquals(expectedEventCount, observer.mEventCount);

        // Modifying any key should trigger an event.
        mSubject.writeString("string_key", "foo");
        expectedEventCount++;
        assertEquals(expectedEventCount, observer.mEventCount);

        // Removing prefixed keys should trigger an event for each one.
        mSubject.writeInt(PREFIXED_KEY_1, 77);
        mSubject.writeInt(PREFIXED_KEY_2, 66);
        mSubject.removeKeysWithPrefix(TEST_PREFIX);
        expectedEventCount += 4; // Two write and two removals.
        assertEquals(expectedEventCount, observer.mEventCount);

        // After removing the observer, it should stop getting events.
        mSubject.removeObserver(observer);
        mSubject.writeString("string_key", "bar");
        assertEquals(expectedEventCount, observer.mEventCount);
    }

    @Test
    @SmallTest
    public void testPrefsAreWipedBetweenTests_1() {
        doTestPrefsAreWipedBetweenTests();
    }

    @Test
    @SmallTest
    public void testPrefsAreWipedBetweenTests_2() {
        doTestPrefsAreWipedBetweenTests();
    }

    /**
     * {@link #testPrefsAreWipedBetweenTests_1()} and {@link #testPrefsAreWipedBetweenTests_2()}
     * each set the same preference and fail if it has been set previously. Whichever order these
     * tests are run, either will fail if the prefs are not wiped between tests.
     */
    private void doTestPrefsAreWipedBetweenTests() {
        // Disable key checking for this test because "dirty_pref" isn't registered in the "in use"
        // list.
        BaseChromePreferenceKeyChecker checkerHeld =
                SharedPreferencesManager.getInstance().swapKeyCheckerForTesting(
                        new BaseChromePreferenceKeyChecker());

        try {
            // If the other test has set this flag and it was not wiped out, fail.
            assertFalse(SharedPreferencesManager.getInstance().readBoolean("dirty_pref", false));

            // Set the flag so the other test ensures it was wiped out.
            SharedPreferencesManager.getInstance().writeBoolean("dirty_pref", true);
        } finally {
            // Restore the key checker.
            SharedPreferencesManager.getInstance().swapKeyCheckerForTesting(checkerHeld);
        }
    }
}
