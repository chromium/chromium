// Copyright 2017 The Chromium Authors
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
import org.chromium.base.test.util.Batch;
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

    @Batch("foo")
    private static class ChildClass extends ParentClass {}

    private static class Groups {
        @ParameterizedCommandLineFlags({
            @Switches({"c1", "c2"}),
            @Switches({"c3", "c4"}),
        })
        public void testA() {}

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
        Description desc =
                Description.createTestDescription(
                        ParentClass.class,
                        "testA",
                        ParentClass.class.getMethod("testA").getAnnotations());
        JSONObject json = TestListInstrumentationRunListener.getTestMethodJSON(desc);
        String expectedJsonString = makeJSON("{", " 'method': 'testA',", " 'annotations': {}", "}");
        Assert.assertEquals(expectedJsonString, json.toString());
    }

    @Test
    public void testGetTestMethodJSON_testB() throws Throwable {
        Description desc =
                Description.createTestDescription(
                        ParentClass.class,
                        "testB",
                        ParentClass.class.getMethod("testB").getAnnotations());
        JSONObject json = TestListInstrumentationRunListener.getTestMethodJSON(desc);
        String expectedJsonString =
                makeJSON(
                        "{",
                        " 'method': 'testB',",
                        " 'annotations': {",
                        "  'CommandLineFlags$Add': {",
                        "   'value': ['world']",
                        "  }",
                        " }",
                        "}");
        Assert.assertEquals(expectedJsonString, json.toString());
    }

    @Test
    public void testGetTestMethodJSONForInheritedClass() throws Throwable {
        Description desc =
                Description.createTestDescription(
                        ChildClass.class,
                        "testB",
                        ChildClass.class.getMethod("testB").getAnnotations());
        JSONObject json = TestListInstrumentationRunListener.getTestMethodJSON(desc);
        String expectedJsonString =
                makeJSON(
                        "{",
                        " 'method': 'testB',",
                        " 'annotations': {",
                        "   'CommandLineFlags$Add': {",
                        "    'value': ['world']",
                        "   }",
                        "  }",
                        "}");
        Assert.assertEquals(expectedJsonString, json.toString());
    }

    @Test
    public void testGetAnnotationJSONForParentClass() throws Throwable {
        JSONObject json =
                TestListInstrumentationRunListener.getAnnotationJSON(
                        Arrays.asList(ParentClass.class.getAnnotations()));
        String expectedJsonString =
                makeJSON("{", " 'CommandLineFlags$Add': {", "  'value': ['hello']", " }", "}");
        Assert.assertEquals(expectedJsonString, json.toString());
    }

    @Test
    public void testGetAnnotationJSONForChildClass() throws Throwable {
        JSONObject json =
                TestListInstrumentationRunListener.getAnnotationJSON(
                        Arrays.asList(ChildClass.class.getAnnotations()));
        String expectedJsonString =
                makeJSON(
                        "{",
                        " 'CommandLineFlags$Add': {",
                        "  'value': ['hello']",
                        " },",
                        " 'Batch': {",
                        "  'value': 'foo'",
                        " }",
                        "}");
        Assert.assertEquals(expectedJsonString, json.toString());
    }

    @Test
    public void testGetTestMethodJSONGroup_testA() throws Throwable {
        Description desc =
                Description.createTestDescription(
                        Groups.class, "testA", Groups.class.getMethod("testA").getAnnotations());
        JSONObject json = TestListInstrumentationRunListener.getTestMethodJSON(desc);
        String expectedJsonString =
                makeJSON(
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
                        "}");
        Assert.assertEquals(expectedJsonString, json.toString());
    }

    @Test
    public void testGetTestMethodJSONGroup_testB() throws Throwable {
        Description desc =
                Description.createTestDescription(
                        Groups.class, "testB", Groups.class.getMethod("testB").getAnnotations());
        JSONObject json = TestListInstrumentationRunListener.getTestMethodJSON(desc);
        String expectedJsonString =
                makeJSON(
                        "{",
                        " 'method': 'testB',",
                        " 'annotations': {",
                        "  'ParameterizedCommandLineFlags': {",
                        "   'value': []",
                        "  }",
                        " }",
                        "}");
        Assert.assertEquals(expectedJsonString, json.toString());
    }
}
