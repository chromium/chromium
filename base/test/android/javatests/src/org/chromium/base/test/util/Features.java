// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.chromium.base.FeatureParam;
import org.chromium.base.Flag;
import org.chromium.base.cached_flags.ValuesReturned;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides annotations for enabling / disabling features during tests.
 *
 * <p>Sample code:
 *
 * <pre>
 * @EnableFeatures(BaseFeatures.FOO)
 * public class Test {
 *
 *    @EnableFeatures(BaseFeatures.BAR + ":paramA/valueA/paramB/valueB")
 *    public void testBarEnabled() { ... }
 *
 *    @EnableFeatures({
 *            BaseFeatures.BAR + ":paramA/valueA/paramB/valueB",
 *            BaseFeatures.BAZ})
 *    public void testBarBazEnabled() { ... }
 *
 *    @DisableFeatures(ContentFeatureList.BAR)
 *    public void testBarDisabled() { ... }
 *
 *    @DisableFeatures({ContentFeatureList.BAR, ContentFeatureList.BAZ})
 *    public void testBarBazDisabled() { ... }
 * }
 * </pre>
 */
public class Features {
    @Retention(RetentionPolicy.RUNTIME)
    public @interface EnableFeatures {
        String[] value();
    }

    @Retention(RetentionPolicy.RUNTIME)
    public @interface DisableFeatures {
        String[] value();
    }

    private Features() {}

    static void resetCachedFlags() {
        ValuesReturned.clearForTesting();
        Flag.resetAllInMemoryCachedValuesForTesting();
        FeatureParam.resetAllInMemoryCachedValuesForTesting();
    }
}
