// Copyright 2019 The Chromium Authors
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

    private static class OtherTaskTraitsExtensionDescriptor
            extends FakeTaskTraitsExtensionDescriptor {
        @Override
        public int getId() {
            return super.getId() + 1;
        }
    }

    private static final FakeTaskTraitsExtensionDescriptor DESC =
            new FakeTaskTraitsExtensionDescriptor();

    private static final OtherTaskTraitsExtensionDescriptor DESC2 =
            new OtherTaskTraitsExtensionDescriptor();

    @Test
    @SmallTest
    public void testExtensionPresent() {
        String input = "Blub";
        TaskTraits traits = TaskTraits.forExtension(TaskPriority.USER_VISIBLE, DESC, input);
        String extension = traits.getExtension(DESC);
        assertEquals(input, extension);
    }

    @Test
    @SmallTest
    public void testExtensionNotPresent() {
        String input = "Blub";
        TaskTraits traits = TaskTraits.forExtension(TaskPriority.USER_VISIBLE, DESC, input);
        String extension = traits.getExtension(DESC2);
        assertNull(extension);
    }
}
