// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import androidx.test.filters.SmallTest;

import com.google.common.collect.Sets;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.shared_preferences.KeyPrefix;
import org.chromium.base.shared_preferences.PreferenceKeyRegistry;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * Test class that verifies that {@link ChromePreferenceKeys} conforms to its constraints: - No keys
 * are both in [keys in use] and in [deprecated keys]. - All keys follow the format
 * "Chrome.[Feature].[Key]"
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromePreferenceKeysTest {
    /**
     * The important test: verify that keys in {@link ChromePreferenceKeys} are not reused, both
     * between registries and across time (checking the deprecated key list).
     *
     * <p>If a key was used in the past but is not used anymore, it should be in [deprecated keys].
     * Adding the same key to [keys in use] will break this test to warn the developer.
     */
    @Test
    @SmallTest
    public void testKeysAreNotReused() {
        // Build sets of all keys combined between registries and check for any intersections
        // between registries.
        Set<String> allKeysInUse = new HashSet<>();
        Set<String> allLegacyFormatKeys = new HashSet<>();
        Set<KeyPrefix> allLegacyFormatPrefixesInUse = new HashSet<>();
        Set<String> allLegacyFormatPrefixesPatternsInUse = new HashSet<>();
        for (PreferenceKeyRegistry registry : AllPreferenceKeyRegistries.KNOWN_REGISTRIES) {
            failIfAnyCommonElements(
                    allKeysInUse,
                    registry.mKeysInUse,
                    "Shared preference keys present in multiple registries");
            failIfAnyCommonElements(
                    allLegacyFormatKeys,
                    registry.mLegacyFormatKeys,
                    "Legacy-format shared preference key present in multiple registries");

            Set<String> newLegacyFormatPrefixesPatterns = new HashSet<>();
            for (KeyPrefix legacyPrefix : registry.mLegacyPrefixes) {
                newLegacyFormatPrefixesPatterns.add(legacyPrefix.pattern());
            }
            failIfAnyCommonElements(
                    allLegacyFormatPrefixesPatternsInUse,
                    newLegacyFormatPrefixesPatterns,
                    "Legacy-format shared preference KeyPrefix present in multiple registries");

            allKeysInUse.addAll(registry.mKeysInUse);
            allLegacyFormatKeys.addAll(registry.mLegacyFormatKeys);
            allLegacyFormatPrefixesInUse.addAll(registry.mLegacyPrefixes);
            allLegacyFormatPrefixesPatternsInUse.addAll(newLegacyFormatPrefixesPatterns);
        }

        doTestKeysAreNotReused(
                allKeysInUse,
                allLegacyFormatKeys,
                DeprecatedChromePreferenceKeys.getKeysForTesting(),
                allLegacyFormatPrefixesInUse,
                DeprecatedChromePreferenceKeys.getPrefixesForTesting());
    }

    private static void failIfAnyCommonElements(
            Set<String> s1, Set<String> s2, String failureMessage) {
        Set<String> intersection = Sets.intersection(s1, s2);
        if (!intersection.isEmpty()) {
            String keyStrings = String.join(",", intersection);
            fail(failureMessage + ": " + keyStrings);
        }
    }

    private void doTestKeysAreNotReused(
            Collection<String> usedList,
            Collection<String> legacyUsedList,
            Collection<String> deprecatedList,
            Collection<KeyPrefix> usedLegacyPrefixList,
            Collection<KeyPrefix> deprecatedLegacyPrefixList) {
        // Check for duplicate keys in [keys in use].
        Set<String> usedSet = new HashSet<>(usedList);
        assertEquals(usedList.size(), usedSet.size());

        Set<String> legacyUsedSet = new HashSet<>(legacyUsedList);
        assertEquals(legacyUsedList.size(), legacyUsedSet.size());

        Set<String> intersection = new HashSet<>(usedSet);
        intersection.retainAll(legacyUsedSet);
        if (!intersection.isEmpty()) {
            fail(
                    "\""
                            + intersection.iterator().next()
                            + "\" is both in ChromePreferenceKeys' regular and legacy "
                            + "[keys in use]");
        }

        Set<String> allKeysInUse = new HashSet<>(usedSet);
        allKeysInUse.addAll(legacyUsedSet);

        // Check for duplicate keys in [deprecated keys].
        Set<String> deprecatedSet = new HashSet<>(deprecatedList);
        assertEquals(deprecatedList.size(), deprecatedSet.size());

        // Check for keys in [deprecated keys] that are now also [keys in use]. This ensures no
        // deprecated keys are reused.
        intersection = new HashSet<>(allKeysInUse);
        intersection.retainAll(deprecatedSet);
        if (!intersection.isEmpty()) {
            fail(
                    "\""
                            + intersection.iterator().next()
                            + "\" is both in ChromePreferenceKeys' [keys in use] and in "
                            + "[deprecated keys]");
        }

        // Check for keys that match a legacy prefix, deprecated or not.
        List<KeyPrefix> legacyPrefixes = new ArrayList<>(usedLegacyPrefixList);
        legacyPrefixes.addAll(deprecatedLegacyPrefixList);

        for (String usedKey : usedSet) {
            for (KeyPrefix legacyPrefix : legacyPrefixes) {
                assertFalse(legacyPrefix.hasGenerated(usedKey));
            }
        }
    }

    // Below are tests to ensure that testKeysAreNotReused() works.

    @Test
    @SmallTest
    public void testReuseCheck_emptyLists() {
        doTestKeysAreNotReused(
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReuseCheck_duplicateKey_used() {
        doTestKeysAreNotReused(
                Arrays.asList("UsedKey1", "UsedKey1"),
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReuseCheck_duplicateKey_legacy() {
        doTestKeysAreNotReused(
                Collections.EMPTY_LIST,
                Arrays.asList("LegacyKey1", "LegacyKey1"),
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReuseCheck_duplicateKey_deprecated() {
        doTestKeysAreNotReused(
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST,
                Arrays.asList("DeprecatedKey1", "DeprecatedKey1"),
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST);
    }

    @Test
    @SmallTest
    public void testReuseCheck_noIntersection() {
        doTestKeysAreNotReused(
                Arrays.asList("UsedKey1", "UsedKey2"),
                Arrays.asList("LegacyKey1", "LegacyKey2"),
                Arrays.asList("DeprecatedKey1", "DeprecatedKey2"),
                Arrays.asList(
                        new KeyPrefix("UsedLegacyFormat1*"), new KeyPrefix("UsedLegacyFormat2*")),
                Arrays.asList(
                        new KeyPrefix("DeprecatedFormat1*"), new KeyPrefix("DeprecatedFormat2*")));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReuseCheck_intersectionUsedAndLegacy() {
        doTestKeysAreNotReused(
                Arrays.asList("ReusedKey", "UsedKey1"),
                Arrays.asList("LegacyKey1", "ReusedKey"),
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReuseCheck_intersectionUsedAndDeprecated() {
        doTestKeysAreNotReused(
                Arrays.asList("UsedKey1", "ReusedKey"),
                Collections.EMPTY_LIST,
                Arrays.asList("ReusedKey", "DeprecatedKey1"),
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReuseCheck_intersectionLegacyAndDeprecated() {
        doTestKeysAreNotReused(
                Collections.EMPTY_LIST,
                Arrays.asList("LegacyKey1", "ReusedKey"),
                Arrays.asList("ReusedKey", "DeprecatedKey1"),
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReuseCheck_intersectionUsedLegacyFormat_prefix() {
        doTestKeysAreNotReused(
                Arrays.asList("UsedKey1"),
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST,
                Arrays.asList(new KeyPrefix("UsedKey*")),
                Collections.EMPTY_LIST);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReuseCheck_intersectionDeprecatedLegacyFormat_prefix() {
        doTestKeysAreNotReused(
                Arrays.asList("UsedKey1"),
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST,
                Collections.EMPTY_LIST,
                Arrays.asList(new KeyPrefix("Used*")));
    }

    /** Test that the keys in use (not legacy) conform to the format: "Chrome.[Feature].[Key]" */
    @Test
    @SmallTest
    public void testKeysConformToFormat() {
        doTestKeysConformToFormat(ChromePreferenceKeys.getKeysInUse());
    }

    /**
     * Old legacy constants are checked to see if they shouldn't be in {@link
     * ChromePreferenceKeys#getKeysInUse()}.
     */
    @Test
    @SmallTest
    public void testLegacyKeysDoNotConformToFormat() {
        doTestKeysDoNotConformToFormat(LegacyChromePreferenceKeys.getKeysInUse());
    }

    private void doTestKeysConformToFormat(List<String> usedList) {
        Pattern regex = buildValidKeyFormatPattern();
        for (String keyInUse : usedList) {
            assertTrue(
                    "\"" + keyInUse + "\" does not conform to format \"Chrome.[Feature].[Key]\"",
                    regex.matcher(keyInUse).matches());
        }
    }

    private void doTestKeysDoNotConformToFormat(List<String> legacyList) {
        Pattern regex = buildValidKeyFormatPattern();
        for (String keyInUse : legacyList) {
            assertFalse(
                    "\""
                            + keyInUse
                            + "\" conforms to format \"Chrome.[Feature].[Key]\", move it to"
                            + " ChromePreferenceKeys.createKeysInUse()",
                    regex.matcher(keyInUse).matches());
        }
    }

    private static Pattern buildValidKeyFormatPattern() {
        String term = "([A-Z][a-z0-9]*)+";
        return Pattern.compile("Chrome\\." + term + "\\." + term + "(\\.\\*)?");
    }

    // Below are tests to ensure that doTestKeysConformToFormat() works.

    private static class TestFormatConstantsClass {
        static final String LEGACY_IN = "legacy_in";
        static final String NEW1 = "Chrome.FeatureOne.Key1";
        static final String NEW2 = "Chrome.Foo.Key";
        static final String BROKEN_PREFIX = "Chrom.Foo.Key";
        static final String MISSING_FEATURE = "Chrome..Key";
        static final String LOWERCASE_KEY = "Chrome.Foo.key";

        static final KeyPrefix PREFIX = new KeyPrefix("Chrome.FeatureOne.KeyPrefix1.*");
        static final KeyPrefix PREFIX_EXTRA_LEVEL =
                new KeyPrefix("Chrome.FeatureOne.KeyPrefix1.ExtraLevel.*");
        static final KeyPrefix PREFIX_MISSING_LEVEL = new KeyPrefix("Chrome.FeatureOne.*");
    }

    @Test
    @SmallTest
    public void testFormatCheck_correct() {
        doTestKeysConformToFormat(
                Arrays.asList(TestFormatConstantsClass.NEW1, TestFormatConstantsClass.NEW2));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testFormatCheck_invalidFormat() {
        doTestKeysConformToFormat(
                Arrays.asList(
                        TestFormatConstantsClass.LEGACY_IN,
                        TestFormatConstantsClass.NEW1,
                        TestFormatConstantsClass.NEW2));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testFormatCheck_brokenPrefix() {
        doTestKeysConformToFormat(
                Arrays.asList(
                        TestFormatConstantsClass.NEW1, TestFormatConstantsClass.BROKEN_PREFIX));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testFormatCheck_missingFeature() {
        doTestKeysConformToFormat(
                Arrays.asList(
                        TestFormatConstantsClass.NEW1, TestFormatConstantsClass.MISSING_FEATURE));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testFormatCheck_lowercaseKey() {
        doTestKeysConformToFormat(
                Arrays.asList(
                        TestFormatConstantsClass.NEW1, TestFormatConstantsClass.LOWERCASE_KEY));
    }

    @Test
    @SmallTest
    public void testFormatCheck_prefixCorrect() {
        doTestKeysConformToFormat(Arrays.asList(TestFormatConstantsClass.PREFIX.pattern()));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testFormatCheck_prefixExtraLevel() {
        doTestKeysConformToFormat(
                Arrays.asList(TestFormatConstantsClass.PREFIX_EXTRA_LEVEL.pattern()));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testFormatCheck_prefixMissingLevel() {
        doTestKeysConformToFormat(
                Arrays.asList(TestFormatConstantsClass.PREFIX_MISSING_LEVEL.pattern()));
    }
}
