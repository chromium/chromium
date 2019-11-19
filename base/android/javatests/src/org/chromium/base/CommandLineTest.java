// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;

/**
 * Tests for {@link CommandLine}.
 * TODO(bauerb): Convert to local JUnit test
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class CommandLineTest {
    // A reference command line. Note that switch2 is [brea\d], switch3 is [and "butter"],
    // and switch4 is [a "quoted" 'food'!]
    static final String INIT_SWITCHES[] = { "init_command", "--SWITCH", "Arg",
        "--switch2=brea\\d", "--switch3=and \"butter\"",
        "--switch4=a \"quoted\" 'food'!",
        "--", "--actually_an_arg" };

    // The same command line, but in quoted string format.
    static final char INIT_SWITCHES_BUFFER[] =
        ("init_command --SWITCH Arg --switch2=brea\\d --switch3=\"and \\\"butt\"er\\\"   "
        + "--switch4='a \"quoted\" \\'food\\'!' "
        + "-- --actually_an_arg").toCharArray();

    static final String CL_ADDED_SWITCH = "zappo-dappo-doggy-trainer";
    static final String CL_ADDED_SWITCH_2 = "username";
    static final String CL_ADDED_VALUE_2 = "bozo";

    @Before
    public void setUp() {
        CommandLine.reset();
    }

    void checkInitSwitches() {
        CommandLine cl = CommandLine.getInstance();
        Assert.assertFalse(cl.hasSwitch("init_command"));
        Assert.assertFalse(cl.hasSwitch("switch"));
        Assert.assertTrue(cl.hasSwitch("SWITCH"));
        Assert.assertFalse(cl.hasSwitch("--SWITCH"));
        Assert.assertFalse(cl.hasSwitch("Arg"));
        Assert.assertFalse(cl.hasSwitch("actually_an_arg"));
        Assert.assertEquals("brea\\d", cl.getSwitchValue("switch2"));
        Assert.assertEquals("and \"butter\"", cl.getSwitchValue("switch3"));
        Assert.assertEquals("a \"quoted\" 'food'!", cl.getSwitchValue("switch4"));
        Assert.assertNull(cl.getSwitchValue("SWITCH"));
        Assert.assertNull(cl.getSwitchValue("non-existant"));
    }

    void checkSettingThenGetting() {
        CommandLine cl = CommandLine.getInstance();

        // Add a plain switch.
        Assert.assertFalse(cl.hasSwitch(CL_ADDED_SWITCH));
        cl.appendSwitch(CL_ADDED_SWITCH);
        Assert.assertTrue(cl.hasSwitch(CL_ADDED_SWITCH));

        // Add a switch paired with a value.
        Assert.assertFalse(cl.hasSwitch(CL_ADDED_SWITCH_2));
        Assert.assertNull(cl.getSwitchValue(CL_ADDED_SWITCH_2));
        cl.appendSwitchWithValue(CL_ADDED_SWITCH_2, CL_ADDED_VALUE_2);
        Assert.assertTrue(CL_ADDED_VALUE_2.equals(cl.getSwitchValue(CL_ADDED_SWITCH_2)));

        // Append a few new things.
        final String switchesAndArgs[] = { "dummy", "--superfast", "--speed=turbo" };
        Assert.assertFalse(cl.hasSwitch("dummy"));
        Assert.assertFalse(cl.hasSwitch("superfast"));
        Assert.assertNull(cl.getSwitchValue("speed"));
        cl.appendSwitchesAndArguments(switchesAndArgs);
        Assert.assertFalse(cl.hasSwitch("dummy"));
        Assert.assertFalse(cl.hasSwitch("command"));
        Assert.assertTrue(cl.hasSwitch("superfast"));
        Assert.assertTrue("turbo".equals(cl.getSwitchValue("speed")));
    }

    void checkTokenizer(String[] expected, String toParse) {
        String[] actual = CommandLine.tokenizeQuotedArguments(toParse.toCharArray());
        Assert.assertEquals(expected.length, actual.length);
        for (int i = 0; i < expected.length; ++i) {
            Assert.assertEquals("comparing element " + i, expected[i], actual[i]);
        }
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testJavaInitialization() {
        CommandLine.init(INIT_SWITCHES);
        checkInitSwitches();
        checkSettingThenGetting();
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testBufferInitialization() {
        CommandLine.init(CommandLine.tokenizeQuotedArguments(INIT_SWITCHES_BUFFER));
        checkInitSwitches();
        checkSettingThenGetting();
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testArgumentTokenizer() {
        String toParse = " a\"\\bc de\\\"f g\"\\h ij    k\" \"lm";
        String[] expected = { "a\\bc de\"f g\\h",
                              "ij",
                              "k lm" };
        checkTokenizer(expected, toParse);

        toParse = "";
        expected = new String[0];
        checkTokenizer(expected, toParse);

        toParse = " \t\n";
        checkTokenizer(expected, toParse);

        toParse = " \"a'b\" 'c\"d' \"e\\\"f\" 'g\\'h' \"i\\'j\" 'k\\\"l'"
                + " m\"n\\'o\"p q'r\\\"s't";
        expected = new String[] { "a'b",
                                  "c\"d",
                                  "e\"f",
                                  "g'h",
                                  "i\\'j",
                                  "k\\\"l",
                                  "mn\\'op",
                                  "qr\\\"st"};
        checkTokenizer(expected, toParse);
    }
}
