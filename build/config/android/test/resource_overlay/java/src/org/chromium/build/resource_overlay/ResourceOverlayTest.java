// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.resource_overlay;

import static org.junit.Assert.assertEquals;

import android.content.res.Resources;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

/** Test for resource_overlay parameter in android_resources() build rule. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ResourceOverlayTest {
    /**
     * Test that when an android_resources() target with resource_overlay=false has a resource with
     * the same name but a different value as a dependency with resource_overlay=true that the value
     * of the resource in the dependency is used.
     */
    @Test
    @SmallTest
    public void testDependencyTagged() {
        Resources resources = InstrumentationRegistry.getTargetContext().getResources();
        assertEquals(41, resources.getInteger(R.integer.resource_overlay_dependency_tagged_secret));
    }

    /**
     * Test that when an android_resources() target with resource_overlay=true has a resource with
     * the same name but different value as one of its dependencies that the value of resource in
     * the target with resource_overlay=true is used.
     */
    @Test
    @SmallTest
    public void testRootTagged() {
        Resources resources = InstrumentationRegistry.getTargetContext().getResources();
        assertEquals(42, resources.getInteger(R.integer.resource_overlay_root_tagged_secret));
    }

    /**
     * Test that when both an android_resources() target and its dependency have
     * resource_overlay=true with resources of the same name but different values, the dependent
     * target's value is used. This tests the fix for dependency ordering in resource overlays.
     */
    @Test
    @SmallTest
    public void testBothTagged() {
        Resources resources = InstrumentationRegistry.getTargetContext().getResources();
        assertEquals(42, resources.getInteger(R.integer.resource_overlay_both_tagged_secret));
    }
}
