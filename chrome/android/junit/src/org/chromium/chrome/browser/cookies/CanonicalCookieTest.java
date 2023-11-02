// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cookies;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * Unit test serialization code.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class CanonicalCookieTest {
    // Name meant to match CanonicalCookie method.
    private static byte[] saveListToStream(final List<CanonicalCookie> cookies)
            throws Exception {
        ByteArrayOutputStream outByteStream = new ByteArrayOutputStream();
        DataOutputStream out = new DataOutputStream(outByteStream);
        try {
            CanonicalCookie cookiesArray[] = new CanonicalCookie[cookies.size()];
            cookies.toArray(cookiesArray);
            CanonicalCookie.saveListToStream(out, cookiesArray);
        } finally {
            out.close();
            outByteStream.close();
        }
        return outByteStream.toByteArray();
    }

    // Name meant to match CanonicalCookie method.
    private static List<CanonicalCookie> readListFromStream(final byte[] byteArray)
            throws Exception {
        ByteArrayInputStream inByteStream = new ByteArrayInputStream(byteArray);
        DataInputStream in = new DataInputStream(inByteStream);
        try {
            return CanonicalCookie.readListFromStream(in);
        } finally {
            in.close();
            inByteStream.close();
        }
    }

    private static void assertCookiesEqual(CanonicalCookie lhs, CanonicalCookie rhs) {
        Assert.assertEquals(lhs.getName(), rhs.getName());
        Assert.assertEquals(lhs.getValue(), rhs.getValue());
        Assert.assertEquals(lhs.getDomain(), rhs.getDomain());
        Assert.assertEquals(lhs.getPath(), rhs.getPath());
        Assert.assertEquals(lhs.getCreationDate(), rhs.getCreationDate());
        Assert.assertEquals(lhs.getExpirationDate(), rhs.getExpirationDate());
        Assert.assertEquals(lhs.getLastAccessDate(), rhs.getLastAccessDate());
        Assert.assertEquals(lhs.getLastUpdateDate(), rhs.getLastUpdateDate());
        Assert.assertEquals(lhs.isSecure(), rhs.isSecure());
        Assert.assertEquals(lhs.isHttpOnly(), rhs.isHttpOnly());
        Assert.assertEquals(lhs.getSameSite(), rhs.getSameSite());
        Assert.assertEquals(lhs.getPriority(), rhs.getPriority());
        Assert.assertEquals(lhs.isSameParty(), rhs.isSameParty());
        Assert.assertEquals(lhs.sourceScheme(), rhs.sourceScheme());
        Assert.assertEquals(lhs.sourcePort(), rhs.sourcePort());
    }

    private static void doSaveRestoreCookiesListTest(final List<CanonicalCookie> cookies)
            throws Exception {
        byte[] byteArray = saveListToStream(cookies);
        List<CanonicalCookie> readCookies = readListFromStream(byteArray);

        Assert.assertEquals(cookies.size(), readCookies.size());
        for (int i = 0; i < cookies.size(); ++i) {
            assertCookiesEqual(cookies.get(i), readCookies.get(i));
        }
    }

    @Test
    public void testSaveRestoreEmptyList() throws Exception {
        doSaveRestoreCookiesListTest(new ArrayList<CanonicalCookie>());
    }

    @Test
    public void testSaveRestore() throws Exception {
        ArrayList<CanonicalCookie> cookies = new ArrayList<>();
        cookies.add(new CanonicalCookie("name", "value", "domain", "path", 0 /* creation */,
                1 /* expiration */, 0 /* lastAccess */, 0 /* lastUpdate */, false /* secure */,
                true /* httpOnly */, 0 /* sameSite */, 0 /* priority */, false /* same_party */,
                "" /* partition_key */, 1 /* sourceScheme */, 72 /* sourcePort */));
        cookies.add(new CanonicalCookie("name2", "value2", ".domain2", "path2", 10 /* creation */,
                20 /* expiration */, 15 /* lastAccess */, 15 /* lastUpdate */, true /* secure */,
                false /* httpOnly */, 1 /* sameSite */, 1 /* priority */, true /* same_party */,
                "" /* partition_key */, 2 /* sourceScheme */, 445 /* sourcePort */));
        cookies.add(new CanonicalCookie("name3", "value3", "domain3", "path3", 10 /* creation */,
                20 /* expiration */, 15 /* lastAccess */, 15 /* lastUpdate */, true /* secure */,
                false /* httpOnly */, 2 /* sameSite */, 2 /* priority */, false /* same_party */,
                "https://toplevelsite.com" /* partition_key */, 2 /* sourceScheme */,
                -1 /* sourcePort */));

        doSaveRestoreCookiesListTest(cookies);
    }
}
