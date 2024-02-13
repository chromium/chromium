// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link Token}. These are derived from token_unittest.cc. Note random tokens are
 * tested in {@link TokenTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class TokenUnitTest {
    @Test
    public void testZeroToken() {
        Token token = new Token(0L, 0L);
        assertEquals(0L, token.getHigh());
        assertEquals(0L, token.getLow());
        assertTrue(token.isZero());
    }

    @Test
    public void testExplicitValueToken() {
        Token token = new Token(1234L, 5678L);
        assertEquals(1234L, token.getHigh());
        assertEquals(5678L, token.getLow());
        assertFalse(token.isZero());
    }

    @Test
    public void testTokenEquality() {
        checkEquals(new Token(0, 0), new Token(0, 0));
        checkEquals(new Token(1, 2), new Token(1, 2));
        assertNotEquals(new Token(1, 2), new Token(1, 3));
        assertNotEquals(new Token(1, 2), new Token(2, 2));
        assertNotEquals(new Token(1, 2), new Token(3, 4));
    }

    @Test
    public void testToString() {
        assertEquals("00000000000000000000000000000000", new Token(0, 0).toString());
        assertEquals("00000000000000010000000000000002", new Token(1, 2).toString());
        assertEquals(
                "0123456789ABCDEF5A5A5A5AA5A5A5A5",
                new Token(0x0123456789abcdefL, 0x5a5a5a5aa5a5a5a5L).toString());
        assertEquals(
                "FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE",
                new Token(0xfffffffffffffffdL, 0xfffffffffffffffeL).toString());
    }

    @Test
    public void testCreateRandom() {
        Token token = Token.createRandom();
        assertFalse(token.isZero());
    }

    private void checkEquals(Token first, Token second) {
        assertEquals(first, second);
        assertEquals(first.hashCode(), second.hashCode());
    }
}
