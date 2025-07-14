// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.ContentResolver;
import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

/** Test class for {@link VirtualDocumentPath}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class VirtualDocumentPathTest {
    // Its document ID is the URL-encoded relative path under context.getCacheDir().
    private static final String DOCPROV_AUTHORITY =
            ContextUtils.getApplicationContext().getPackageName() + ".docprov";

    @After
    public void tearDown() {
        FileUtils.recursivelyDeleteFile(ContextUtils.getApplicationContext().getCacheDir(), null);
    }

    @Test
    @SmallTest
    public void testParseSuccess() {
        String[] validPaths = {
            "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir-id",
            "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir-id/",
            "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir-id/a/b",
            "/SAF/" + DOCPROV_AUTHORITY + "/tree/Downloads%2FMyFolder",
            "content://" + DOCPROV_AUTHORITY + "/tree/dir-id",
            "content://" + DOCPROV_AUTHORITY + "/tree/Downloads%2FMyFolder"
        };
        for (String path : validPaths) {
            Assert.assertNotNull(path, VirtualDocumentPath.parse(path));
        }
    }

    @Test
    @SmallTest
    public void testParseFail() {
        String[] invalidPaths = {
            "",
            "/",
            "/SAF",
            "/SAF/" + DOCPROV_AUTHORITY + "/tree/", // Incomplete
            "/SAF/" + DOCPROV_AUTHORITY + "/foo/dir-id", // Missing "/tree/"
            "SAF/" + DOCPROV_AUTHORITY + "/tree/dir-id", // Missing leading slash
            "/SAF//tree/dir-id", // Missing authority
            "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir-id/a//b", // Empty subpath component
            "content://" + DOCPROV_AUTHORITY + "/document/dir-id", // Not a tree URI
            // Tree URI with a document ID
            "content://" + DOCPROV_AUTHORITY + "/tree/dir-id/document/dir-id%2Fchild",
            "content:///tree/dir-id", // No authority
            "content://" + DOCPROV_AUTHORITY + "/tree/", // No document ID
            "file://" + DOCPROV_AUTHORITY + "/tree/dir-id"
        };
        for (String path : invalidPaths) {
            Assert.assertNull(path, VirtualDocumentPath.parse(path));
        }
    }

    @Test
    @SmallTest
    public void testToString() {
        String[][] testCases = {
            {
                "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir-id",
                "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir-id"
            },
            {
                "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir-id/",
                "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir-id"
            },
            {
                "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir-id/a/b",
                "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir-id/a/b"
            },
            {
                "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir-id/a/b/",
                "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir-id/a/b"
            },
            {
                "content://" + DOCPROV_AUTHORITY + "/tree/dir-id",
                "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir-id"
            },
            {
                "content://" + DOCPROV_AUTHORITY + "/tree/Downloads%2FMyFolder",
                "/SAF/" + DOCPROV_AUTHORITY + "/tree/Downloads%2FMyFolder"
            },
        };
        for (String[] testCase : testCases) {
            String path = testCase[0];
            String expected = testCase[1];
            Assert.assertEquals(path, expected, VirtualDocumentPath.parse(path).toString());
        }
    }

    @Test
    @SmallTest
    public void testResolveToContentUriString() {
        Context context = ContextUtils.getApplicationContext();

        File root = context.getCacheDir();
        Assert.assertTrue(new File(root, "dir/a/a").mkdirs());
        Assert.assertTrue(new File(root, "dir/a/b").mkdirs());

        String[][] testCases = {
            {
                "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir",
                "content://" + DOCPROV_AUTHORITY + "/tree/dir/document/dir"
            },
            {
                "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir/a",
                "content://" + DOCPROV_AUTHORITY + "/tree/dir/document/dir%2Fa"
            },
            {
                "/SAF/" + DOCPROV_AUTHORITY + "/tree/dir/a/b",
                "content://" + DOCPROV_AUTHORITY + "/tree/dir/document/dir%2Fa%2Fb"
            },
            {"/SAF/" + DOCPROV_AUTHORITY + "/tree/no-such-dir", ""},
            {"/SAF/" + DOCPROV_AUTHORITY + "/tree/no-such-dir/a", ""},
        };

        for (String[] testCase : testCases) {
            String path = testCase[0];
            String expected = testCase[1];
            VirtualDocumentPath vp = VirtualDocumentPath.parse(path);
            Assert.assertEquals(path, expected, vp.resolveToContentUriString());
        }
    }

    @Test
    @SmallTest
    public void testMkdir() {
        Context context = ContextUtils.getApplicationContext();

        File root = context.getCacheDir();
        Assert.assertTrue(new File(root, "dir").mkdir());

        VirtualDocumentPath a =
                VirtualDocumentPath.parse("/SAF/" + DOCPROV_AUTHORITY + "/tree/dir/a");
        VirtualDocumentPath ab =
                VirtualDocumentPath.parse("/SAF/" + DOCPROV_AUTHORITY + "/tree/dir/a/b");

        Assert.assertFalse(ab.mkdir()); // No dir/a

        Assert.assertEquals("", a.resolveToContentUriString());

        Assert.assertTrue(a.mkdir()); // dir/a created

        Assert.assertEquals(
                "content://" + DOCPROV_AUTHORITY + "/tree/dir/document/dir%2Fa",
                a.resolveToContentUriString());

        Assert.assertFalse(a.mkdir()); // dir/a already exists

        Assert.assertTrue(ab.mkdir()); // dir/a/b created
    }

    @Test
    @SmallTest
    public void testWrite() throws IOException {
        Context context = ContextUtils.getApplicationContext();
        ContentResolver resolver = context.getContentResolver();

        File root = context.getCacheDir();
        Assert.assertTrue(new File(root, "dir/a").mkdirs());

        VirtualDocumentPath txt =
                VirtualDocumentPath.parse("/SAF/" + DOCPROV_AUTHORITY + "/tree/dir/x.txt");
        VirtualDocumentPath a =
                VirtualDocumentPath.parse("/SAF/" + DOCPROV_AUTHORITY + "/tree/dir/a");
        VirtualDocumentPath btxt =
                VirtualDocumentPath.parse("/SAF/" + DOCPROV_AUTHORITY + "/tree/dir/b/x.txt");

        Assert.assertTrue(txt.writeFile(new byte[] {'a'}));
        InputStream i = resolver.openInputStream(txt.resolveToContentUri());
        Assert.assertEquals('a', i.read());
        Assert.assertEquals(-1, i.read());
        i.close();

        Assert.assertTrue(txt.writeFile(new byte[] {'b'}));
        i = resolver.openInputStream(txt.resolveToContentUri());
        Assert.assertEquals('b', i.read());
        Assert.assertEquals(-1, i.read());
        i.close();

        Assert.assertFalse(a.writeFile(new byte[] {'a'}));
        Assert.assertFalse(btxt.writeFile(new byte[] {'a'}));
    }
}
