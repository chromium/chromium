// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.app.Instrumentation;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.params.ParameterizedCommandLineFlags;
import org.chromium.base.test.params.ParameterizedCommandLineFlags.Switches;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.testing.TestListInstrumentationRunListener;

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

    private TestListInstrumentationRunListener mListener;
    private Bundle mLastBundle;

    @Before
    public void setUp() {
        mListener = new TestListInstrumentationRunListener();
        mListener.setInstrumentation(
                new Instrumentation() {
                    @Override
                    public void sendStatus(int resultCode, Bundle results) {
                        mLastBundle = results;
                    }
                });
    }

    @Test
    public void testGetTestMethodJSON_testA() throws Throwable {
        Description desc =
                Description.createTestDescription(
                        ParentClass.class,
                        "testA",
                        ParentClass.class.getMethod("testA").getAnnotations());
        mListener.testFinished(desc);
        Bundle expected = new Bundle();
        expected.putString(
                "class",
                "org.chromium.base.test.TestListInstrumentationRunListenerTest$ParentClass");
        expected.putString(
                "class_annotations", "{\"CommandLineFlags$Add\":{\"value\":[\"hello\"]}}");
        expected.putString("method", "testA");
        expected.putString("method_annotations", "{}");
        Assert.assertEquals(expected.toString(), mLastBundle.toString());
        mListener.testFinished(desc);
        expected.remove("class");
        expected.remove("class_annotations");
        Assert.assertEquals(expected.toString(), mLastBundle.toString());
    }

    @Test
    public void testGetTestMethodJSON_testB() throws Throwable {
        Description desc =
                Description.createTestDescription(
                        ParentClass.class,
                        "testB",
                        ParentClass.class.getMethod("testB").getAnnotations());
        mListener.testFinished(desc);
        Bundle expected = new Bundle();
        expected.putString(
                "class",
                "org.chromium.base.test.TestListInstrumentationRunListenerTest$ParentClass");
        expected.putString(
                "class_annotations", "{\"CommandLineFlags$Add\":{\"value\":[\"hello\"]}}");
        expected.putString("method", "testB");
        expected.putString(
                "method_annotations", "{\"CommandLineFlags$Add\":{\"value\":[\"world\"]}}");
        Assert.assertEquals(expected.toString(), mLastBundle.toString());
    }

    @Test
    public void testGetTestMethodJSONForInheritedClass() throws Throwable {
        Description desc =
                Description.createTestDescription(
                        ChildClass.class,
                        "testB",
                        ChildClass.class.getMethod("testB").getAnnotations());
        mListener.testFinished(desc);
        Bundle expected = new Bundle();
        expected.putString(
                "class",
                "org.chromium.base.test.TestListInstrumentationRunListenerTest$ChildClass");
        expected.putString(
                "class_annotations",
                "{\"CommandLineFlags$Add\":{\"value\":[\"hello\"]},\"Batch\":{\"value\":\"foo\"}}");
        expected.putString("method", "testB");
        expected.putString(
                "method_annotations", "{\"CommandLineFlags$Add\":{\"value\":[\"world\"]}}");
        Assert.assertEquals(expected.toString(), mLastBundle.toString());
    }

    @Test
    public void testGetTestMethodJSONGroup_testA() throws Throwable {
        Description desc =
                Description.createTestDescription(
                        Groups.class, "testA", Groups.class.getMethod("testA").getAnnotations());
        mListener.testFinished(desc);
        Bundle expected = new Bundle();
        expected.putString(
                "class", "org.chromium.base.test.TestListInstrumentationRunListenerTest$Groups");
        expected.putString("class_annotations", "{}");
        expected.putString("method", "testA");
        expected.putString(
                "method_annotations",
                """
                {"ParameterizedCommandLineFlags":{"value":[\
                {"ParameterizedCommandLineFlags$Switches":{"value":["c1","c2"]}},\
                {"ParameterizedCommandLineFlags$Switches":{"value":["c3","c4"]}}]}}\
                """);

        Assert.assertEquals(expected.toString(), mLastBundle.toString());
    }

    @Test
    public void testGetTestMethodJSONGroup_testB() throws Throwable {
        Description desc =
                Description.createTestDescription(
                        Groups.class, "testB", Groups.class.getMethod("testB").getAnnotations());
        mListener.testFinished(desc);
        Bundle expected = new Bundle();
        expected.putString(
                "class", "org.chromium.base.test.TestListInstrumentationRunListenerTest$Groups");
        expected.putString("class_annotations", "{}");
        expected.putString("method", "testB");
        expected.putString(
                "method_annotations", "{\"ParameterizedCommandLineFlags\":{\"value\":[]}}");
        Assert.assertEquals(expected.toString(), mLastBundle.toString());
    }
}
