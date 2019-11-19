// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.params.ParameterizedCommandLineFlags;
import org.chromium.base.test.params.ParameterizedCommandLineFlags.Switches;
import org.chromium.base.test.util.CommandLineFlags;

import java.util.Arrays;

/**
 * Robolectric test to ensure static methods in TestListInstrumentationRunListener works properly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TestListInstrumentationRunListenerTest {
    @CommandLineFlags.Add("hello")
    private static class ParentClass {
        public void testA() {}

        @CommandLineFlags.Add("world")
        public void testB() {}
    }

    @CommandLineFlags.Remove("hello")
    private static class ChildClass extends ParentClass {
    }

    private static class Groups {
        // clang-format off
        @ParameterizedCommandLineFlags({
            @Switches({"c1", "c2"}),
            @Switches({"c3", "c4"}),
        })
        public void testA() {}
        // clang-format on
        @ParameterizedCommandLineFlags
        public void testB() {}
    }

    private String makeJSON(String... lines) {
        StringBuilder builder = new StringBuilder();
        for (String line : lines) {
            builder.append(line);
        }
        return builder.toString().replaceAll("\\s", "").replaceAll("'", "\"");
    }

    @Test
    public void testGetTestMethodJSON_testA() throws Throwable {
        Description desc = Description.createTestDescription(
                ParentClass.class, "testA",
                ParentClass.class.getMethod("testA").getAnnotations());
        JSONObject json = TestListInstrumentationRunListener.getTestMethodJSON(desc);
        // clang-format off
        String expectedJsonString = makeJSON(
            "{",
            " 'method': 'testA',",
            " 'annotations': {}",
            "}"
        );
        // clang-format on
        Assert.assertEquals(expectedJsonString, json.toString());
    }

    @Test
    public void testGetTestMethodJSON_testB() throws Throwable {
        Description desc = Description.createTestDescription(
                ParentClass.class, "testB",
                ParentClass.class.getMethod("testB").getAnnotations());
        JSONObject json = TestListInstrumentationRunListener.getTestMethodJSON(desc);
        // clang-format off
        String expectedJsonString = makeJSON(
            "{",
            " 'method': 'testB',",
            " 'annotations': {",
            "  'CommandLineFlags$Add': {",
            "   'value': ['world']",
            "  }",
            " }",
            "}"
        );
        // clang-format on
        Assert.assertEquals(expectedJsonString, json.toString());
    }


    @Test
    public void testGetTestMethodJSONForInheritedClass() throws Throwable {
        Description desc = Description.createTestDescription(
                ChildClass.class, "testB",
                ChildClass.class.getMethod("testB").getAnnotations());
        JSONObject json = TestListInstrumentationRunListener.getTestMethodJSON(desc);
        // clang-format off
        String expectedJsonString = makeJSON(
            "{",
            " 'method': 'testB',",
            " 'annotations': {",
            "   'CommandLineFlags$Add': {",
            "    'value': ['world']",
            "   }",
            "  }",
            "}"
        );
        // clang-format on
        Assert.assertEquals(expectedJsonString, json.toString());
    }

    @Test
    public void testGetAnnotationJSONForParentClass() throws Throwable {
        JSONObject json = TestListInstrumentationRunListener.getAnnotationJSON(
                Arrays.asList(ParentClass.class.getAnnotations()));
        // clang-format off
        String expectedJsonString = makeJSON(
            "{",
            " 'CommandLineFlags$Add': {",
            "  'value': ['hello']",
            " }",
            "}"
        );
        // clang-format on
        Assert.assertEquals(expectedJsonString, json.toString());
    }

    @Test
    public void testGetAnnotationJSONForChildClass() throws Throwable {
        JSONObject json = TestListInstrumentationRunListener.getAnnotationJSON(
                Arrays.asList(ChildClass.class.getAnnotations()));
        // clang-format off
        String expectedJsonString = makeJSON(
            "{",
            " 'CommandLineFlags$Add': {",
            "  'value': ['hello']",
            " },",
            " 'CommandLineFlags$Remove': {",
            "  'value': ['hello']",
            " }",
            "}"
        );
        // clang-format on
        Assert.assertEquals(expectedJsonString, json.toString());
    }

    @Test
    public void testGetTestMethodJSONGroup_testA() throws Throwable {
        Description desc = Description.createTestDescription(
                Groups.class, "testA", Groups.class.getMethod("testA").getAnnotations());
        JSONObject json = TestListInstrumentationRunListener.getTestMethodJSON(desc);
        // clang-format off
        String expectedJsonString = makeJSON(
            "{",
            " 'method': 'testA',",
            " 'annotations': {",
            "  'ParameterizedCommandLineFlags': {",
            "   'value': [",
            "    {",
            "     'ParameterizedCommandLineFlags$Switches': {",
            "      'value': ['c1','c2']",
            "     }",
            "    },",
            "    {",
            "     'ParameterizedCommandLineFlags$Switches': {",
            "      'value': ['c3','c4']",
            "     }",
            "    }",
            "   ]",
            "  }",
            " }",
            "}"
        );
        // clang-format on
        Assert.assertEquals(expectedJsonString, json.toString());
    }

    @Test
    public void testGetTestMethodJSONGroup_testB() throws Throwable {
        Description desc = Description.createTestDescription(
                Groups.class, "testB", Groups.class.getMethod("testB").getAnnotations());
        JSONObject json = TestListInstrumentationRunListener.getTestMethodJSON(desc);
        // clang-format off
        String expectedJsonString = makeJSON(
            "{",
            " 'method': 'testB',",
            " 'annotations': {",
            "  'ParameterizedCommandLineFlags': {",
            "   'value': []",
            "  }",
            " }",
            "}"
        );
        // clang-format on
        Assert.assertEquals(expectedJsonString, json.toString());
    }
}

