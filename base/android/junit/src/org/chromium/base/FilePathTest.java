// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link FilePath}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FilePathTest {

    @Test
    public void testIsAbsolute() {
        assertTrue(FilePath.from("/").isAbsolute());
        assertTrue(FilePath.from("/foo").isAbsolute());
        assertTrue(FilePath.from("/foo/bar").isAbsolute());

        assertFalse(FilePath.from("").isAbsolute());
        assertFalse(FilePath.from("foo").isAbsolute());
        assertFalse(FilePath.from("foo/bar").isAbsolute());
        assertFalse(FilePath.from("./foo").isAbsolute());
        assertFalse(FilePath.from("../foo").isAbsolute());
    }

    @Test
    public void testReferencesParent() {
        assertTrue(FilePath.from("..").referencesParent());
        assertTrue(FilePath.from("../").referencesParent());
        assertTrue(FilePath.from("foo/..").referencesParent());
        assertTrue(FilePath.from("../foo").referencesParent());
        assertTrue(FilePath.from("foo/../bar").referencesParent());
        assertTrue(FilePath.from("/..").referencesParent());
        assertTrue(FilePath.from("/../foo").referencesParent());
        assertTrue(FilePath.from("foo/./../bar").referencesParent());
        assertTrue(FilePath.from("foo//../bar").referencesParent());

        assertFalse(FilePath.from("").referencesParent());
        assertFalse(FilePath.from(".").referencesParent());
        assertFalse(FilePath.from("./").referencesParent());
        assertFalse(FilePath.from("foo").referencesParent());
        assertFalse(FilePath.from("foo/bar").referencesParent());
        assertFalse(FilePath.from("foo..bar").referencesParent());
        assertFalse(FilePath.from("..foo").referencesParent());
        assertFalse(FilePath.from("foo..").referencesParent());
        assertFalse(FilePath.from("foo/..bar").referencesParent());
        assertFalse(FilePath.from("foo/bar..").referencesParent());
    }

    @Test
    public void testNullTruncation() {
        assertTrue(FilePath.from("/foo\0..").isAbsolute());
        assertFalse(FilePath.from("/foo\0..").referencesParent());

        assertFalse(FilePath.from("foo\0/bar").isAbsolute());
        assertFalse(FilePath.from("foo\0/bar").referencesParent());

        assertTrue(FilePath.from("/\0..").isAbsolute());
        assertFalse(FilePath.from("/\0..").referencesParent());

        assertFalse(FilePath.from("\0/foo").isAbsolute());
        assertFalse(FilePath.from("foo\0..").referencesParent());
        assertTrue(FilePath.from("/foo/../bar\0/../baz").referencesParent());
    }
}
