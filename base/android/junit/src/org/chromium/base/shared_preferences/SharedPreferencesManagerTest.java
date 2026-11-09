// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.shared_preferences;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.SharedPreferences;

import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.BuildConfig;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Unit tests for {@link SharedPreferencesManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SharedPreferencesManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private PreferenceKeyChecker mChecker;

    private static final KeyPrefix TEST_PREFIX = new KeyPrefix("TestPrefix.*");
    private static final String PREFIXED_KEY_1 = TEST_PREFIX.createKey("stemA");
    private static final String PREFIXED_KEY_2 = TEST_PREFIX.createKey("stemB");
    private static final String PREFIXED_KEY_3 = TEST_PREFIX.createKey(33);

    private SharedPreferencesManager mSubject;

    @Before
    public void setUp() {
        mSubject = new SharedPreferencesManager(mChecker);
    }

    @Test
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
    public void testIncrementInt() {
        mSubject.writeInt("int_key", 100);
        int result = mSubject.incrementInt("int_key");

        assertEquals(101, result);
        assertEquals(101, mSubject.readInt("int_key"));
    }

    @Test
    public void testIncrementIntDefault() {
        int result = mSubject.incrementInt("int_key");

        assertEquals(1, result);
        assertEquals(1, mSubject.readInt("int_key"));
    }

    @Test
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
    public void testAddToStringSet() {
        mSubject.writeStringSet("string_set_key", new HashSet<>(Collections.singletonList("bar")));
        mSubject.addToStringSet("string_set_key", "foo");

        assertEquals(
                new HashSet<>(Arrays.asList("foo", "bar")),
                mSubject.readStringSet("string_set_key"));
    }

    @Test
    public void testAddToStringSetDefault() {
        mSubject.addToStringSet("string_set_key", "foo");

        assertEquals(
                new HashSet<>(Collections.singletonList("foo")),
                mSubject.readStringSet("string_set_key"));
    }

    @Test
    public void testRemoveFromStringSet() {
        mSubject.writeStringSet("string_set_key", new HashSet<>(Arrays.asList("foo", "bar")));
        mSubject.removeFromStringSet("string_set_key", "foo");

        assertEquals(
                new HashSet<>(Collections.singletonList("bar")),
                mSubject.readStringSet("string_set_key"));
    }

    @Test
    public void testRemoveFromStringSetDefault() {
        mSubject.removeFromStringSet("string_set_key", "foo");

        assertEquals(Collections.emptySet(), mSubject.readStringSet("string_set_key"));
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testReadStringSet_nonEmpty_returnsUnmodifiable() {
        Set<String> exampleStringSet = new HashSet<>(Arrays.asList("d", "e"));
        mSubject.writeStringSet("string_set_key", exampleStringSet);

        Set<String> unmodifiableSet = mSubject.readStringSet("string_set_key");

        // Should throw an exception
        unmodifiableSet.add("f");
    }

    @Test
    public void testWriteIntSync() throws InterruptedException {
        // Verify default return values when no value is written.
        assertEquals(0, mSubject.readInt("int_key"));

        // Write a value on a background thread.
        Thread t = new Thread(() -> mSubject.writeIntSync("int_key", 123));
        t.start();
        t.join();

        // Verify value written can be read.
        assertEquals(123, mSubject.readInt("int_key"));
    }

    @Test
    public void testWriteBooleanSync() throws InterruptedException {
        // Verify default return values when no value is written.
        assertEquals(false, mSubject.readBoolean("bool_key", false));

        // Write a value on a background thread.
        Thread t = new Thread(() -> mSubject.writeBooleanSync("bool_key", true));
        t.start();
        t.join();

        // Verify value written can be read.
        assertEquals(true, mSubject.readBoolean("bool_key", false));
    }

    @Test
    public void testWriteStringSync() throws InterruptedException {
        // Verify default return values when no value is written.
        assertEquals("default", mSubject.readString("string_key", "default"));

        // Write a value on a background thread.
        Thread t = new Thread(() -> mSubject.writeStringSync("string_key", "foo"));
        t.start();
        t.join();

        // Verify value written can be read.
        assertEquals("foo", mSubject.readString("string_key", "default"));
    }

    @Test
    public void testWriteLongSync() throws InterruptedException {
        // Verify default return values when no value is written.
        assertEquals(0, mSubject.readLong("long_key"));

        // Write a value on a background thread.
        Thread t = new Thread(() -> mSubject.writeLongSync("long_key", 9999999999L));
        t.start();
        t.join();

        // Verify value written can be read.
        assertEquals(9999999999L, mSubject.readLong("long_key"));
    }

    @Test
    public void testWriteFloatSync() throws InterruptedException {
        // Verify default return values when no value is written.
        assertEquals(0f, mSubject.readFloat("float_key", 0f), 0f);

        // Write a value on a background thread.
        Thread t = new Thread(() -> mSubject.writeFloatSync("float_key", 42.42f));
        t.start();
        t.join();

        // Verify value written can be read.
        assertEquals(42.42f, mSubject.readFloat("float_key", 1.5f), 0.001f);
    }

    @Test
    public void testRemoveKeySync() throws InterruptedException {
        // Write a value.
        mSubject.writeInt("int_key", 123);
        assertEquals(123, mSubject.readInt("int_key", 999));

        // Write the value on a background thread.
        Thread t = new Thread(() -> mSubject.removeKeySync("int_key"));
        t.start();
        t.join();

        // Verify value was removed.
        assertEquals(999, mSubject.readInt("int_key", 999));
    }

    @Test
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
    public void testRemoveKeys_2() {
        // Write some values
        mSubject.writeInt(TEST_PREFIX.createKey("infix1"), 111);
        mSubject.writeInt(TEST_PREFIX.createKey("infix2"), 222);
        mSubject.writeInt(TEST_PREFIX.createKey("infix3"), 333);
        mSubject.writeInt(PREFIXED_KEY_1, 444);
        mSubject.writeInt(PREFIXED_KEY_2, 555);
        mSubject.writeInt(PREFIXED_KEY_3, 666);

        // Remove some of them
        mSubject.removeKeysWithPrefix(TEST_PREFIX, "infix");

        // Verify only values with the given prefix were removed.
        assertEquals(0, mSubject.readInt(TEST_PREFIX.createKey("infix1"), 0));
        assertEquals(0, mSubject.readInt(TEST_PREFIX.createKey("infix2"), 0));
        assertEquals(0, mSubject.readInt(TEST_PREFIX.createKey("infix3"), 0));
        assertEquals(444, mSubject.readInt(PREFIXED_KEY_1, 0));
        assertEquals(555, mSubject.readInt(PREFIXED_KEY_2, 0));
        assertEquals(666, mSubject.readInt(PREFIXED_KEY_3, 0));
    }

    @Test
    public void testSetStringsWithPrefix() {
        // Write some values.
        mSubject.writeString(PREFIXED_KEY_1, "first");
        mSubject.writeString(PREFIXED_KEY_2, "second");
        mSubject.writeString(PREFIXED_KEY_3, "third");
        mSubject.writeString("OtherKey", "fourth");

        // Set values with prefix.
        mSubject.setStringsWithPrefix(TEST_PREFIX, "fifth");

        // Verify values written are read with readStringsWithPrefix().
        Map<String, String> result = mSubject.readStringsWithPrefix(TEST_PREFIX);
        assertEquals(3, result.size());

        assertEquals("fifth", result.get(PREFIXED_KEY_1));
        assertEquals("fifth", result.get(PREFIXED_KEY_2));
        assertEquals("fifth", result.get(PREFIXED_KEY_3));

        // Verify that the other key is unchanged.
        assertEquals("fourth", mSubject.readString("OtherKey", ""));
    }

    @Test
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
    public void testSetIntsWithPrefix() {
        // Write some values.
        mSubject.writeInt(PREFIXED_KEY_1, 1);
        mSubject.writeInt(PREFIXED_KEY_2, 2);
        mSubject.writeInt(PREFIXED_KEY_3, 3);
        mSubject.writeInt("OtherKey", 4);

        // Set values with prefix.
        mSubject.setIntsWithPrefix(TEST_PREFIX, 5);

        // Verify values written are read with readIntsWithPrefix().
        Map<String, Integer> result = mSubject.readIntsWithPrefix(TEST_PREFIX);
        assertEquals(3, result.size());
        assertEquals(5, result.get(PREFIXED_KEY_1).intValue());
        assertEquals(5, result.get(PREFIXED_KEY_2).intValue());
        assertEquals(5, result.get(PREFIXED_KEY_3).intValue());

        // Verify that the other key is unchanged.
        assertEquals(4, mSubject.readInt("OtherKey"));
    }

    @Test
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
    public void testSetLongsWithPrefix() {
        // Write some values.
        mSubject.writeLong(PREFIXED_KEY_1, 21474836470001L);
        mSubject.writeLong(PREFIXED_KEY_2, 21474836470002L);
        mSubject.writeLong(PREFIXED_KEY_3, 21474836470003L);
        mSubject.writeLong("OtherKey", 21474836470004L);

        // Set values with prefix.
        mSubject.setLongsWithPrefix(TEST_PREFIX, 21474836470005L);

        // Verify values written are read with readLongsWithPrefix().
        Map<String, Long> result = mSubject.readLongsWithPrefix(TEST_PREFIX);
        assertEquals(3, result.size());
        assertEquals(21474836470005L, result.get(PREFIXED_KEY_1).longValue());
        assertEquals(21474836470005L, result.get(PREFIXED_KEY_2).longValue());
        assertEquals(21474836470005L, result.get(PREFIXED_KEY_3).longValue());

        // Verify that the other key is unchanged.
        assertEquals(21474836470004L, mSubject.readLong("OtherKey"));
    }

    @Test
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
    public void testSetFloatsWithPrefix() {
        // Write some values.
        mSubject.writeFloat(PREFIXED_KEY_1, 1.0f);
        mSubject.writeFloat(PREFIXED_KEY_2, 2.5f);
        mSubject.writeFloat(PREFIXED_KEY_3, 3.5f);
        mSubject.writeFloat("OtherKey", 4.0f);

        // Set values with prefix.
        mSubject.setFloatsWithPrefix(TEST_PREFIX, 5.0f);

        // Verify values written are read with readFloatsWithPrefix().
        Map<String, Float> result = mSubject.readFloatsWithPrefix(TEST_PREFIX);
        assertEquals(3, result.size());
        assertEquals(5.0f, result.get(PREFIXED_KEY_1), 1e-10);
        assertEquals(5.0f, result.get(PREFIXED_KEY_2), 1e-10);
        assertEquals(5.0f, result.get(PREFIXED_KEY_3), 1e-10);

        // Verify that the other key is unchanged.
        assertEquals(4.0f, mSubject.readFloat("OtherKey", 0f), 1e-10);
    }

    @Test
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
    public void testSetDoublesWithPrefix() {
        // Write some values.
        mSubject.writeDouble(PREFIXED_KEY_1, 1.0);
        mSubject.writeDouble(PREFIXED_KEY_2, 2.5);
        mSubject.writeDouble(PREFIXED_KEY_3, 3.5);
        mSubject.writeDouble("OtherKey", 4.0);

        // Set values with prefix.
        mSubject.setDoublesWithPrefix(TEST_PREFIX, 5.0);

        // Verify values written are read with readDoublesWithPrefix().
        Map<String, Double> result = mSubject.readDoublesWithPrefix(TEST_PREFIX);
        assertEquals(3, result.size());
        assertEquals(5.0, result.get(PREFIXED_KEY_1), 1e-10);
        assertEquals(5.0, result.get(PREFIXED_KEY_2), 1e-10);
        assertEquals(5.0, result.get(PREFIXED_KEY_3), 1e-10);

        // Verify that the other key is unchanged.
        assertEquals(4.0, mSubject.readDouble("OtherKey", 0d), 1e-10);
    }

    @Test
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
    public void testSetBooleansWithPrefix() {
        // Write some values.
        mSubject.writeBoolean(PREFIXED_KEY_1, true);
        mSubject.writeBoolean(PREFIXED_KEY_2, false);
        mSubject.writeBoolean(PREFIXED_KEY_3, true);
        mSubject.writeBoolean("OtherKey", true);

        // Set values with prefix.
        mSubject.setBooleansWithPrefix(TEST_PREFIX, false);

        // Verify values written are read with readBooleansWithPrefix().
        Map<String, Boolean> result = mSubject.readBooleansWithPrefix(TEST_PREFIX);
        assertEquals(3, result.size());
        assertFalse(result.get(PREFIXED_KEY_1));
        assertFalse(result.get(PREFIXED_KEY_2));
        assertFalse(result.get(PREFIXED_KEY_3));

        // Verify that the other key is unchanged.
        assertTrue(mSubject.readBoolean("OtherKey", false));
    }

    @Test
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
    public void testCheckerIsCalled() {
        mSubject.writeInt("int_key", 123);
        verify(mChecker, times(1)).checkIsKeyInUse(eq("int_key"));
        mSubject.readInt("int_key");
        verify(mChecker, times(2)).checkIsKeyInUse(eq("int_key"));
        mSubject.incrementInt("int_key");
        verify(mChecker, times(3)).checkIsKeyInUse(eq("int_key"));

        mSubject.writeBoolean("bool_key", true);
        verify(mChecker, times(1)).checkIsKeyInUse(eq("bool_key"));
        mSubject.readBoolean("bool_key", false);
        verify(mChecker, times(2)).checkIsKeyInUse(eq("bool_key"));

        mSubject.writeString("string_key", "foo");
        verify(mChecker, times(1)).checkIsKeyInUse(eq("string_key"));
        mSubject.readString("string_key", "");
        verify(mChecker, times(2)).checkIsKeyInUse(eq("string_key"));

        mSubject.writeLong("long_key", 999L);
        verify(mChecker, times(1)).checkIsKeyInUse(eq("long_key"));
        mSubject.readLong("long_key");
        verify(mChecker, times(2)).checkIsKeyInUse(eq("long_key"));

        mSubject.writeFloat("float_key", 2.5f);
        verify(mChecker, times(1)).checkIsKeyInUse(eq("float_key"));
        mSubject.readFloat("float_key", 0f);
        verify(mChecker, times(2)).checkIsKeyInUse(eq("float_key"));

        mSubject.writeDouble("double_key", 2.5d);
        verify(mChecker, times(1)).checkIsKeyInUse(eq("double_key"));
        mSubject.readDouble("double_key", 0d);
        verify(mChecker, times(2)).checkIsKeyInUse(eq("double_key"));

        mSubject.writeStringSet("string_set_key", new HashSet<>());
        verify(mChecker, times(1)).checkIsKeyInUse(eq("string_set_key"));
        mSubject.readStringSet("string_set_key");
        verify(mChecker, times(2)).checkIsKeyInUse(eq("string_set_key"));
        mSubject.addToStringSet("string_set_key", "bar");
        verify(mChecker, times(3)).checkIsKeyInUse(eq("string_set_key"));
        mSubject.removeFromStringSet("string_set_key", "bar");
        verify(mChecker, times(4)).checkIsKeyInUse(eq("string_set_key"));

        mSubject.removeKey("some_key");
        verify(mChecker, times(1)).checkIsKeyInUse(eq("some_key"));
        mSubject.contains("some_key");
        verify(mChecker, times(2)).checkIsKeyInUse(eq("some_key"));

        mSubject.readBooleansWithPrefix(TEST_PREFIX);
        verify(mChecker, times(1)).checkIsPrefixInUse(eq(TEST_PREFIX));
        mSubject.readIntsWithPrefix(TEST_PREFIX);
        verify(mChecker, times(2)).checkIsPrefixInUse(eq(TEST_PREFIX));
        mSubject.readLongsWithPrefix(TEST_PREFIX);
        verify(mChecker, times(3)).checkIsPrefixInUse(eq(TEST_PREFIX));
        mSubject.readFloatsWithPrefix(TEST_PREFIX);
        verify(mChecker, times(4)).checkIsPrefixInUse(eq(TEST_PREFIX));
        mSubject.readDoublesWithPrefix(TEST_PREFIX);
        verify(mChecker, times(5)).checkIsPrefixInUse(eq(TEST_PREFIX));
        mSubject.readStringsWithPrefix(TEST_PREFIX);
        verify(mChecker, times(6)).checkIsPrefixInUse(eq(TEST_PREFIX));
        mSubject.removeKeysWithPrefix(TEST_PREFIX);
        verify(mChecker, times(7)).checkIsPrefixInUse(eq(TEST_PREFIX));
        mSubject.removeKeysWithPrefix(TEST_PREFIX, "some_key");
        verify(mChecker, times(8)).checkIsPrefixInUse(eq(TEST_PREFIX));

        mSubject.setBooleansWithPrefix(TEST_PREFIX, true);
        verify(mChecker, times(9)).checkIsPrefixInUse(eq(TEST_PREFIX));
        mSubject.setIntsWithPrefix(TEST_PREFIX, 1);
        verify(mChecker, times(10)).checkIsPrefixInUse(eq(TEST_PREFIX));
        mSubject.setLongsWithPrefix(TEST_PREFIX, 1L);
        verify(mChecker, times(11)).checkIsPrefixInUse(eq(TEST_PREFIX));
        mSubject.setFloatsWithPrefix(TEST_PREFIX, 1f);
        verify(mChecker, times(12)).checkIsPrefixInUse(eq(TEST_PREFIX));
        mSubject.setDoublesWithPrefix(TEST_PREFIX, 1d);
        verify(mChecker, times(13)).checkIsPrefixInUse(eq(TEST_PREFIX));
        mSubject.setStringsWithPrefix(TEST_PREFIX, "");
        verify(mChecker, times(14)).checkIsPrefixInUse(eq(TEST_PREFIX));
    }

    @Test
    public void testCheckerIsCalledInEditor() {
        final SharedPreferences.Editor ed = mSubject.getEditor();
        ed.putInt("int_key", 123);
        verify(mChecker).checkIsKeyInUse(eq("int_key"));

        ed.putBoolean("bool_key", true);
        verify(mChecker).checkIsKeyInUse(eq("bool_key"));

        ed.putString("string_key", "foo");
        verify(mChecker).checkIsKeyInUse(eq("string_key"));

        ed.putLong("long_key", 999L);
        verify(mChecker).checkIsKeyInUse(eq("long_key"));

        ed.putFloat("float_key", 2.5f);
        verify(mChecker).checkIsKeyInUse(eq("float_key"));

        ed.putStringSet("string_set_key", new HashSet<>());
        verify(mChecker).checkIsKeyInUse(eq("string_set_key"));

        ed.remove("some_key");
        verify(mChecker).checkIsKeyInUse(eq("some_key"));
    }

    // Named-file support.

    private static final String FILE_A = "SharedPreferencesManagerTest.a";
    private static final String FILE_B = "SharedPreferencesManagerTest.b";

    /** A registry that accepts exactly |legacyKeys| (legacy-format keys like "int_key"). */
    private static PreferenceKeyRegistry registryWith(String... legacyKeys) {
        return new PreferenceKeyRegistry(
                "testModule",
                Collections.emptyList(),
                Arrays.asList(legacyKeys),
                Collections.emptyList());
    }

    private static SharedPreferences rawPrefs(String fileName) {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(fileName, Context.MODE_PRIVATE);
    }

    @Test
    public void testDefaultManager_usesAppSharedPreferences() {
        mSubject.writeInt("int_key", 7);

        assertEquals(7, ContextUtils.getAppSharedPreferences().getInt("int_key", 0));
        assertFalse(rawPrefs(FILE_A).contains("int_key"));
    }

    @Test
    public void testNamedManager_usesNamedFile() {
        SharedPreferencesManager managerA = new SharedPreferencesManager(FILE_A, mChecker);

        managerA.writeInt("int_key", 11);
        managerA.writeString("string_key", "a");
        managerA.writeStringSet("string_set_key", new HashSet<>(Arrays.asList("x", "y")));

        // Visible through the raw file, not through the default file.
        assertEquals(11, rawPrefs(FILE_A).getInt("int_key", 0));
        assertEquals("a", rawPrefs(FILE_A).getString("string_key", null));
        assertEquals(
                new HashSet<>(Arrays.asList("x", "y")),
                rawPrefs(FILE_A).getStringSet("string_set_key", null));
        assertFalse(ContextUtils.getAppSharedPreferences().contains("int_key"));

        // And read back through the manager, including prefix and removal paths.
        assertEquals(11, managerA.readInt("int_key"));
        assertTrue(managerA.contains("string_key"));
        managerA.removeKey("string_key");
        assertFalse(rawPrefs(FILE_A).contains("string_key"));
        managerA.getEditor().putBoolean("bool_key", true).apply();
        assertTrue(rawPrefs(FILE_A).getBoolean("bool_key", false));
    }

    @Test
    public void testNamedManagers_differentFilesAreIsolated() {
        SharedPreferencesManager managerA = new SharedPreferencesManager(FILE_A, mChecker);
        SharedPreferencesManager managerB = new SharedPreferencesManager(FILE_B, mChecker);

        managerA.writeInt("int_key", 1);
        managerB.writeInt("int_key", 2);
        managerA.writeInt(PREFIXED_KEY_1, 10);
        managerB.writeInt(PREFIXED_KEY_2, 20);

        assertEquals(1, managerA.readInt("int_key"));
        assertEquals(2, managerB.readInt("int_key"));
        assertEquals(0, mSubject.readInt("int_key"));

        // Prefix operations only see their own file.
        assertEquals(
                Collections.singleton(PREFIXED_KEY_1),
                managerA.readIntsWithPrefix(TEST_PREFIX).keySet());
        managerB.removeKeysWithPrefix(TEST_PREFIX);
        assertEquals(10, managerA.readInt(PREFIXED_KEY_1));
        assertEquals(0, managerB.readInt(PREFIXED_KEY_2));
    }

    @Test
    public void testGetInstance_sameNameAndRegistryReturnsCachedInstance() {
        PreferenceKeyRegistry registry = registryWith("int_key");

        SharedPreferencesManager first = SharedPreferencesManager.getInstance(FILE_A, registry);
        SharedPreferencesManager second = SharedPreferencesManager.getInstance(FILE_A, registry);
        SharedPreferencesManager other = SharedPreferencesManager.getInstance(FILE_B, registry);

        assertSame(first, second);
        assertNotSame(first, other);
        first.writeInt("int_key", 5);
        assertEquals(5, second.readInt("int_key"));
        assertEquals(0, other.readInt("int_key"));
    }

    @Test
    public void testGetInstance_isDistinctFromDefaultFileManager() {
        PreferenceKeyRegistry registry = registryWith("int_key");

        SharedPreferencesManager named = SharedPreferencesManager.getInstance(FILE_A, registry);
        SharedPreferencesManager defaultManager =
                SharedPreferencesManager.getInstanceForRegistry(registry);

        assertNotSame(named, defaultManager);
        named.writeInt("int_key", 5);
        assertEquals(0, defaultManager.readInt("int_key"));
        assertEquals(0, ContextUtils.getAppSharedPreferences().getInt("int_key", 0));
    }

    @Test
    public void testGetInstanceForRegistry_isCachedAndUsesDefaultFile() {
        PreferenceKeyRegistry registry = registryWith("int_key");

        SharedPreferencesManager first = SharedPreferencesManager.getInstanceForRegistry(registry);
        SharedPreferencesManager second = SharedPreferencesManager.getInstanceForRegistry(registry);

        assertSame(first, second);
        first.writeInt("int_key", 9);
        assertEquals(9, ContextUtils.getAppSharedPreferences().getInt("int_key", 0));
    }

    @Test
    public void testGetInstance_defaultFileNameReturnsGetInstanceForRegistryInstance() {
        // Passing the default file name to getInstance() normalizes to the default-file manager.
        PreferenceKeyRegistry registry = registryWith("int_key");

        SharedPreferencesManager byRegistry =
                SharedPreferencesManager.getInstanceForRegistry(registry);
        SharedPreferencesManager byName =
                SharedPreferencesManager.getInstance(
                        ContextUtils.getDefaultSharedPreferencesName(), registry);

        assertSame(byRegistry, byName);
    }

    @Test
    public void testGetInstance_checksKeysAgainstRegistry() {
        // Key checking only exists in builds with asserts enabled.
        Assume.assumeTrue(BuildConfig.ENABLE_ASSERTS);
        SharedPreferencesManager named =
                SharedPreferencesManager.getInstance(FILE_A, registryWith("int_key"));

        RuntimeException e =
                assertThrows(RuntimeException.class, () -> named.writeInt("unregistered_key", 1));
        assertTrue(e.getMessage().contains("unregistered_key"));
    }

    @Test
    public void testGetInstance_twoRegistriesOnSameFileShareBackingPrefsAndEnforceOwnKeys() {
        Assume.assumeTrue(BuildConfig.ENABLE_ASSERTS);
        PreferenceKeyRegistry registry1 = registryWith("int_key", "shared_key");
        PreferenceKeyRegistry registry2 = registryWith("string_key", "shared_key");

        SharedPreferencesManager manager1 = SharedPreferencesManager.getInstance(FILE_A, registry1);
        SharedPreferencesManager manager2 = SharedPreferencesManager.getInstance(FILE_A, registry2);

        assertNotSame(manager1, manager2);

        // Writes through one manager are visible through the other on the shared file.
        manager1.writeInt("shared_key", 42);
        assertEquals(42, manager2.readInt("shared_key"));

        // Each manager rejects keys registered only in the other registry.
        RuntimeException e1 =
                assertThrows(RuntimeException.class, () -> manager1.readString("string_key", null));
        assertTrue(e1.getMessage().contains("string_key"));
        RuntimeException e2 =
                assertThrows(RuntimeException.class, () -> manager2.readInt("int_key"));
        assertTrue(e2.getMessage().contains("int_key"));
    }

    @Test
    public void testGetDefaultSharedPreferencesName_matchesAppSharedPreferences() {
        String name = ContextUtils.getDefaultSharedPreferencesName();
        assertTrue(name.endsWith("_preferences"));

        ContextUtils.getAppSharedPreferences().edit().putInt("int_key", 3).commit();
        assertEquals(3, rawPrefs(name).getInt("int_key", 0));
    }

    @Test
    public void testResetForTesting_dropsNamedManagersOnly() {
        PreferenceKeyRegistry registry = registryWith("int_key");
        SharedPreferencesManager defaultManager =
                SharedPreferencesManager.getInstanceForRegistry(registry);
        SharedPreferencesManager named = SharedPreferencesManager.getInstance(FILE_A, registry);

        SharedPreferencesManager.resetForTesting();

        assertSame(defaultManager, SharedPreferencesManager.getInstanceForRegistry(registry));
        assertNotSame(named, SharedPreferencesManager.getInstance(FILE_A, registry));
    }

    // Like ChromeSharedPreferences.REGISTRY, one registry instance shared by every test, so the
    // cached manager for FILE_A is the same key in each test.
    private static final PreferenceKeyRegistry SHARED_REGISTRY = registryWith("int_key");

    /**
     * Checks that the named manager reads and writes this test's prefs rather than the previous
     * test's. Robolectric creates a new Application per test, so without
     * SharedPreferencesManager.resetForTesting() in BaseRobolectricTestRule, whichever of the two
     * tests below runs second would get a manager bound to the first test's prefs.
     */
    private static void assertNamedManagerUsesCurrentTestPrefs() {
        SharedPreferencesManager manager =
                SharedPreferencesManager.getInstance(FILE_A, SHARED_REGISTRY);
        assertFalse(manager.contains("int_key"));
        manager.writeInt("int_key", 1);
        assertEquals(1, rawPrefs(FILE_A).getInt("int_key", 0));
    }

    @Test
    public void testNamedManager_notReusedAcrossTests1() {
        assertNamedManagerUsesCurrentTestPrefs();
    }

    @Test
    public void testNamedManager_notReusedAcrossTests2() {
        assertNamedManagerUsesCurrentTestPrefs();
    }
}
