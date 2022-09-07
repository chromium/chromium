// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.Statement;

import java.util.ArrayList;
import java.util.List;

/**
 * Test SetUpStatement is working as intended with SetUpTestRule.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class SetUpStatementTest {
    private Statement mBase;
    private SetUpTestRule<TestRule> mRule;
    private List<Integer> mList;

    @Before
    public void setUp() {
        mBase = new Statement() {
            @Override
            public void evaluate() {
                mList.add(1);
            }
        };
        mList = new ArrayList<>();
        mRule = new SetUpTestRule<TestRule>() {
            @Override
            public void setUp() {
                mList.add(0);
            }

            @Override
            public TestRule shouldSetUp(boolean toSetUp) {
                return null;
            }
        };
    }

    @Test
    public void testSetUpStatementShouldSetUp() throws Throwable {
        SetUpStatement statement = new SetUpStatement(mBase, mRule, true);
        statement.evaluate();
        Integer[] expected = {0, 1};
        Assert.assertArrayEquals(expected, mList.toArray());
    }

    @Test
    public void testSetUpStatementShouldNotSetUp() throws Throwable {
        SetUpStatement statement = new SetUpStatement(mBase, mRule, false);
        statement.evaluate();
        Integer[] expected = {1};
        Assert.assertArrayEquals(expected, mList.toArray());
    }
}
