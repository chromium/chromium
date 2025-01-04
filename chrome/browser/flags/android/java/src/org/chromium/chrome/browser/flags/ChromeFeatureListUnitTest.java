// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.google.common.collect.Sets;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.cached_flags.CachedFeatureParam;
import org.chromium.components.cached_flags.CachedFlag;

import java.lang.reflect.Field;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/** Tests the behavior of {@link ChromeFeatureList}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeFeatureListUnitTest {
    private static final double EPSILON = 1e-7f;

    /** In unit tests, all flags checked must have their value specified. */
    @Test(expected = IllegalArgumentException.class)
    public void testNoOverridesDefaultDisabled_throws() {
        ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_DISABLED);
    }

    /** In unit tests, all flags checked must have their value specified. */
    @Test(expected = IllegalArgumentException.class)
    public void testNoOverridesDefaultEnabled_throws() {
        ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_ENABLED);
    }

    /** In unit tests, flags may have their value specified by the EnableFeatures annotation. */
    @Test
    @EnableFeatures(ChromeFeatureList.TEST_DEFAULT_DISABLED)
    public void testAnnotationEnabled_returnsEnabled() {
        assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_DISABLED));
    }

    /** In unit tests, flags may have their value specified by the DisableFeatures annotation. */
    @Test
    @DisableFeatures(ChromeFeatureList.TEST_DEFAULT_ENABLED)
    public void testAnnotationDisabled_returnsDisabled() {
        assertFalse(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_ENABLED));
    }

    /**
     * In unit tests, flags may have their value specified by calling {@link
     * FeatureOverrides#enable(String)}.
     */
    @Test
    public void testFeatureOverridesEnable_returnsEnabled() {
        FeatureOverrides.enable(ChromeFeatureList.TEST_DEFAULT_DISABLED);
        assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_DISABLED));
    }

    /**
     * In unit tests, flags may have their value specified by calling {@link
     * FeatureOverrides#disable(String)}.
     */
    @Test
    public void testSetTestFeaturesDisabled_returnsDisabled() {
        FeatureOverrides.disable(ChromeFeatureList.TEST_DEFAULT_ENABLED);
        assertFalse(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_ENABLED));
    }

    /**
     * In unit tests, flags may have their param values specified by the EnableFeatures annotation.
     */
    @Test
    @EnableFeatures(
            ChromeFeatureList.TEST_DEFAULT_DISABLED
                    + ":stringParam/stringValue/intParam/2/doubleParam/3.5/booleanParam/true")
    public void testAnnotationEnabledWithParams_returnsParams() {
        assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_DISABLED));
        assertEquals(
                "stringValue",
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.TEST_DEFAULT_DISABLED, "stringParam"));
        assertEquals(
                2,
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.TEST_DEFAULT_DISABLED, "intParam", -1));
        assertEquals(
                3.5,
                ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                        ChromeFeatureList.TEST_DEFAULT_DISABLED, "doubleParam", -1.0),
                EPSILON);
        assertEquals(
                true,
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.TEST_DEFAULT_DISABLED, "booleanParam", false));
    }

    @Test
    public void testAllCachedFlagsMap_matchesCachedFlagsDeclared() throws IllegalAccessException {
        HashSet<String> cachedFlagsDeclared = new HashSet<>();
        for (Field field : ChromeFeatureList.class.getDeclaredFields()) {
            if (CachedFlag.class.isAssignableFrom(field.getType())) {
                CachedFlag flag = (CachedFlag) field.get(null);
                cachedFlagsDeclared.add(flag.getFeatureName());
            }
        }

        Set<String> cachedFlagsListed = ChromeFeatureList.sAllCachedFlags.keySet();

        Set<String> declaredButNotListed = Sets.difference(cachedFlagsDeclared, cachedFlagsListed);
        assertEquals(
                "Cached flags declared in ChromeFeatureList, but not added to |sAllCachedFlags|",
                Collections.emptySet(),
                declaredButNotListed);

        Set<String> listedButNotDeclared = Sets.difference(cachedFlagsListed, cachedFlagsDeclared);
        assertEquals(
                "Cached flags listed in |sAllCachedFlags|, but not declared in ChromeFeatureList",
                Collections.emptySet(),
                listedButNotDeclared);
    }

    @Test
    public void testParamsCached_matchesCachedParamsDeclared() throws IllegalAccessException {
        HashSet<String> cachedParamsDeclared = new HashSet<>();
        for (Field field : ChromeFeatureList.class.getDeclaredFields()) {
            if (CachedFeatureParam.class.isAssignableFrom(field.getType())) {
                CachedFeatureParam<?> param = (CachedFeatureParam<?>) field.get(null);
                cachedParamsDeclared.add(param.getFeatureName() + ":" + param.getName());
            }
        }

        Set<String> cachedParamsListed = new HashSet<>();
        for (CachedFeatureParam<?> param : ChromeFeatureList.sParamsCached) {
            cachedParamsListed.add(param.getFeatureName() + ":" + param.getName());
        }

        Set<String> declaredButNotListed =
                Sets.difference(cachedParamsDeclared, cachedParamsListed);
        assertEquals(
                "Cached params declared in ChromeFeatureList, but not added to |sParamsCached|",
                Collections.emptySet(),
                declaredButNotListed);

        Set<String> listedButNotDeclared =
                Sets.difference(cachedParamsListed, cachedParamsDeclared);
        assertEquals(
                "Cached params listed in |sParamsCached|, but not declared in ChromeFeatureList",
                Collections.emptySet(),
                listedButNotDeclared);
    }
}
