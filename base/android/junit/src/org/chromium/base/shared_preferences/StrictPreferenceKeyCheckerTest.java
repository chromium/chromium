// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.shared_preferences;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link StrictPreferenceKeyChecker}. */
@RunWith(BaseRobolectricTestRunner.class)
public class StrictPreferenceKeyCheckerTest {
    private static final String KEY1_IN_USE = "Chrome.Feature.Key1";
    private static final String KEY2_IN_USE = "Chrome.Feature.Key2";
    private static final String KEY3_NOT_IN_USE = "Chrome.Feature.Key3";
    private static final KeyPrefix KEY_PREFIX1_IN_USE =
            new KeyPrefix("Chrome.Feature.KeyPrefix1.*");
    private static final KeyPrefix KEY_PREFIX2_IN_USE =
            new KeyPrefix("Chrome.Feature.KeyPrefix2.*");
    private static final KeyPrefix KEY_PREFIX3_NOT_IN_USE =
            new KeyPrefix("Chrome.Feature.KeyPrefix3.*");
    private static final String LEGACY_KEY_IN_USE = "legacykey";
    private static final String LEGACY_PREFIX_IN_USE = "legacyprefix_";

    private StrictPreferenceKeyChecker mSubject;

    @Before
    public void setUp() {
        List<String> keysInUse =
                Arrays.asList(
                        KEY1_IN_USE,
                        KEY2_IN_USE,
                        KEY_PREFIX1_IN_USE.pattern(),
                        KEY_PREFIX2_IN_USE.pattern());
        List<String> legacyKeys = Arrays.asList(LEGACY_KEY_IN_USE);
        List<KeyPrefix> legacyPrefixes = Arrays.asList(new KeyPrefix(LEGACY_PREFIX_IN_USE + "*"));
        PreferenceKeyRegistry registry =
                new PreferenceKeyRegistry("testModule", keysInUse, legacyKeys, legacyPrefixes);
        mSubject = new StrictPreferenceKeyChecker(registry);
    }

    @Test
    @SmallTest
    public void testRegularKeys_registered_noException() {
        mSubject.checkIsKeyInUse(KEY1_IN_USE);
        mSubject.checkIsKeyInUse(KEY2_IN_USE);
        mSubject.checkIsKeyInUse(LEGACY_KEY_IN_USE);
        mSubject.checkIsKeyInUse(LEGACY_PREFIX_IN_USE + "restofkey");
    }

    @Test(expected = RuntimeException.class)
    @SmallTest
    public void testRegularKeys_notRegistered_throwsException() {
        mSubject.checkIsKeyInUse(KEY3_NOT_IN_USE);
    }

    @Test
    @SmallTest
    public void testPrefixedKeys_noException() {
        mSubject.checkIsKeyInUse(KEY_PREFIX1_IN_USE.createKey("restofkey"));
    }

    @Test
    @SmallTest
    public void testPrefixedKeys_multipleLevels_noException() {
        mSubject.checkIsKeyInUse(
                KEY_PREFIX2_IN_USE.createKey("ExtraLevel.DynamicallyGenerated98765"));
    }

    @Test(expected = RuntimeException.class)
    @SmallTest
    public void testPrefixedKeys_noPrefixMatch_throwsException() {
        mSubject.checkIsKeyInUse(KEY_PREFIX3_NOT_IN_USE.createKey("restofkey"));
    }

    @Test(expected = RuntimeException.class)
    @SmallTest
    public void testPrefixedKeys_matchOnlyPrefix_throwsException() {
        mSubject.checkIsKeyInUse(KEY_PREFIX1_IN_USE.createKey(""));
    }

    @Test(expected = RuntimeException.class)
    @SmallTest
    public void testPrefixedKeys_matchPattern_throwsException() {
        mSubject.checkIsKeyInUse(KEY_PREFIX1_IN_USE.createKey("*"));
    }

    @Test
    @SmallTest
    public void testPrefix_inUse_noException() {
        mSubject.checkIsPrefixInUse(KEY_PREFIX2_IN_USE);
    }

    @Test(expected = RuntimeException.class)
    @SmallTest
    public void testPrefix_notInUse_throwsException() {
        mSubject.checkIsPrefixInUse(KEY_PREFIX3_NOT_IN_USE);
    }
}
