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

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.cached_flags.CachedFlag;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/** Tests the behavior of {@link ChromeFeatureList}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeFeatureListWithProcessorUnitTest {

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
     * FeatureList#setTestFeatures(java.util.Map)}.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.TEST_DEFAULT_DISABLED)
    public void testSetTestFeaturesEnabled_returnsEnabled() {
        assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_DISABLED));
    }

    /**
     * In unit tests, flags may have their value specified by calling {@link
     * FeatureList#setTestFeatures(java.util.Map)}.
     */
    @Test
    @DisableFeatures(ChromeFeatureList.TEST_DEFAULT_ENABLED)
    public void testSetTestFeaturesDisabled_returnsDisabled() {
        assertFalse(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_ENABLED));
    }

    @Test
    public void testAllCachedFlagsMap_matchesCachedFlagsDeclared() throws IllegalAccessException {
        HashSet<String> cachedFlagsDeclared = new HashSet<>();
        for (Field field : ChromeFeatureList.class.getDeclaredFields()) {
            int modifiers = field.getModifiers();
            if (CachedFlag.class.isAssignableFrom(field.getType())
                    && Modifier.isPublic(modifiers)
                    && Modifier.isStatic(modifiers)
                    && Modifier.isFinal(modifiers)) {
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
                "Cached flags listed in |sAllCachedFlags|, but not declared as public static "
                        + "final in ChromeFeatureList",
                Collections.emptySet(),
                listedButNotDeclared);
    }
}
