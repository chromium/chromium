// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests for {@link TaskTraits}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TaskTraitsTest {
    private static class FakeTaskTraitsExtensionDescriptor
            implements TaskTraitsExtensionDescriptor<String> {
        public static final byte ID = 1;

        @Override
        public int getId() {
            return ID;
        }

        @Override
        public String fromSerializedData(byte[] data) {
            return new String(data);
        }

        @Override
        public byte[] toSerializedData(String extension) {
            return extension.getBytes();
        }
    }

    private static final FakeTaskTraitsExtensionDescriptor DESC =
            new FakeTaskTraitsExtensionDescriptor();

    @Test
    @SmallTest
    public void testExtensionPresent() {
        String input = "Blub";
        TaskTraits traits = TaskTraits.USER_VISIBLE.mayBlock();
        traits.mExtensionId = FakeTaskTraitsExtensionDescriptor.ID;
        traits.mExtensionData = input.getBytes();
        String extension = traits.getExtension(DESC);
        assertEquals(input, extension);
    }

    @Test
    @SmallTest
    public void testExtensionNotPresent() {
        String input = "Blub";
        TaskTraits traits = TaskTraits.USER_VISIBLE.mayBlock();
        traits.mExtensionId = 3;
        traits.mExtensionData = input.getBytes();
        String extension = traits.getExtension(DESC);
        assertNull(extension);
    }

    @Test
    @SmallTest
    public void testSerializeDeserialize() {
        String input = "Blub";
        TaskTraits traits = TaskTraits.USER_VISIBLE;
        String extension = traits.withExtension(DESC, input).getExtension(DESC);
        assertEquals(input, extension);
    }
}
