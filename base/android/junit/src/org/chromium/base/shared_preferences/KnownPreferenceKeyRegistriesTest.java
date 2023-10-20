// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.shared_preferences;

import static org.junit.Assert.fail;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link KnownPreferenceKeyRegistries}. */
@RunWith(BaseRobolectricTestRunner.class)
public class KnownPreferenceKeyRegistriesTest {
    private static final String KEY_1 = "Chrome.Feature.Key1";
    private static final PreferenceKeyRegistry KNOWN_1 =
            createRegistryWithOneKey("known_registry1", KEY_1);

    private static final String KEY_2 = "Chrome.Feature.Key2";
    private static final PreferenceKeyRegistry KNOWN_2 =
            createRegistryWithOneKey("known_registry2", KEY_2);

    private static final String KEY_3 = "Chrome.Feature.Key3";
    private static final PreferenceKeyRegistry UNKNOWN =
            createRegistryWithOneKey("unknown_registry", KEY_3);

    @Before
    public void setUp() {
        KnownPreferenceKeyRegistries.clearForTesting();
    }

    @Test
    public void testOnlyKnownUsedAfterInit_noAssertion() {
        KnownPreferenceKeyRegistries.initializeKnownRegistries(Set.of(KNOWN_1, KNOWN_2));

        SharedPreferencesManager.getInstanceForRegistry(KNOWN_1).writeInt(KEY_1, 42);
        SharedPreferencesManager.getInstanceForRegistry(KNOWN_2).writeInt(KEY_2, 43);
    }

    @Test
    public void testOnlyKnownUsedBeforeInit_noAssertion() {
        SharedPreferencesManager.getInstanceForRegistry(KNOWN_1).writeInt(KEY_1, 42);
        SharedPreferencesManager.getInstanceForRegistry(KNOWN_2).writeInt(KEY_2, 43);

        KnownPreferenceKeyRegistries.initializeKnownRegistries(Set.of(KNOWN_1, KNOWN_2));
    }

    @Test
    public void testUnknownUsedAfterInit_assertion() {
        KnownPreferenceKeyRegistries.initializeKnownRegistries(Set.of(KNOWN_1, KNOWN_2));

        try {
            SharedPreferencesManager.getInstanceForRegistry(UNKNOWN).writeInt(KEY_3, 42);
        } catch (AssertionError e) {
            assertContains("An unknown registry was used", e.getMessage());
            assertContains("unknown_registry", e.getMessage());
            return;
        }
        fail("Expected AssertionError");
    }

    @Test
    public void testUnknownUsedBeforeInit_assertion() {
        SharedPreferencesManager.getInstanceForRegistry(UNKNOWN).writeInt(KEY_3, 42);

        try {
            KnownPreferenceKeyRegistries.initializeKnownRegistries(Set.of(KNOWN_1, KNOWN_2));
        } catch (AssertionError e) {
            assertContains("Unknown registries were used", e.getMessage());
            assertContains("unknown_registry", e.getMessage());
            return;
        }
        fail("Expected AssertionError");
    }

    private static PreferenceKeyRegistry createRegistryWithOneKey(String name, String key) {
        return new PreferenceKeyRegistry(
                name, List.of(key), Collections.EMPTY_LIST, Collections.EMPTY_LIST);
    }

    // TODO: Unify with HistogramWatcherTestBase's version.
    protected static void assertContains(String expectedSubstring, String actualString) {
        Assert.assertNotNull(actualString);
        if (!actualString.contains(expectedSubstring)) {
            fail(
                    String.format(
                            "Substring <%s> not found in string <%s>",
                            expectedSubstring, actualString));
        }
    }
}
