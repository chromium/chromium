// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.support.test.internal.runner.listener.InstrumentationRunListener;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.runner.Description;
import org.junit.runner.notification.Failure;
import org.junit.runners.model.InitializationError;

import org.chromium.base.Log;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.lang.annotation.Annotation;
import java.lang.reflect.Array;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * A RunListener that list out all the test information into a json file.
 */
public class TestListInstrumentationRunListener extends InstrumentationRunListener {
    private static final String TAG = "TestListRunListener";
    private static final Set<String> SKIP_METHODS = new HashSet<>(
            Arrays.asList(new String[] {"toString", "hashCode", "annotationType", "equals"}));

    private final Map<Class<?>, JSONObject> mTestClassJsonMap = new HashMap<>();
    private Failure mFirstFailure;

    @Override
    public void testFailure(Failure failure) {
        if (mFirstFailure == null) {
            mFirstFailure = failure;
        }
    }

    @Override
    public void testFinished(Description desc) throws Exception {
        Class<?> testClass = desc.getTestClass();
        JSONObject classEntry = mTestClassJsonMap.get(testClass);
        if (classEntry == null) {
            classEntry =
                    new JSONObject()
                            .put("class", testClass.getName())
                            .put("superclass", testClass.getSuperclass().getName())
                            .put("annotations",
                                    getAnnotationJSON(Arrays.asList(testClass.getAnnotations())))
                            .put("methods", new JSONArray());
            mTestClassJsonMap.put(testClass, classEntry);
        }
        ((JSONArray) classEntry.get("methods")).put(getTestMethodJSON(desc));
    }

    /**
     * Store the test method description to a Map at the beginning of a test
     * run.
     */
    @Override
    public void testStarted(Description desc) throws Exception {
        // BaseJUnit4ClassRunner only fires testFinished(), so a call to
        // testStarted means a different runner is active, and the test is
        // actually being executed rather than just listed.
        throw new InitializationError("All tests must use"
                + " @RunWith(BaseJUnit4ClassRunner.class) or a subclass thereof."
                + " Found that this test does not: " + desc.getTestClass());
    }

    /**
     * Create a JSONArray with all the test class JSONObjects and save it to
     * listed output path.
     */
    public void saveTestsToJson(String outputPath) throws IOException {
        if (mFirstFailure != null) {
            throw new RuntimeException(
                    "Failed on " + mFirstFailure.getDescription(), mFirstFailure.getException());
        }

        try (Writer writer = new OutputStreamWriter(new FileOutputStream(outputPath), "UTF-8")) {
            JSONArray allTestClassesJSON = new JSONArray(mTestClassJsonMap.values());
            writer.write(allTestClassesJSON.toString());
        } catch (IOException e) {
            Log.e(TAG, "failed to write json to file", e);
            throw e;
        }
    }

    /**
     * Return a JSONOject that represent a Description of a method".
     */
    static JSONObject getTestMethodJSON(Description desc) throws Exception {
        return new JSONObject()
                .put("method", desc.getMethodName())
                .put("annotations", getAnnotationJSON(desc.getAnnotations()));
    }

    /**
     * Make a JSONObject dictionary out of annotations, keyed by the
     * Annotation types' simple java names.
     *
     * For example, for the following group of annotations for ExampleClass
     * <code>
     * @A
     * @B(message = "hello", level = 3)
     * public class ExampleClass() {}
     * </code>
     *
     * This method would return a JSONObject as such:
     * <code>
     * {
     *   "A": {},
     *   "B": {
     *     "message": "hello",
     *     "level": "3"
     *   }
     * }
     * </code>
     *
     * The method accomplish this by though through each annotation and
     * reflectively call the annotation's method to get the element value,
     * with exceptions to methods like "equals()" or "hashCode".
     */
    static JSONObject getAnnotationJSON(Collection<Annotation> annotations)
            throws IllegalAccessException, InvocationTargetException, JSONException {
        JSONObject result = new JSONObject();
        for (Annotation a : annotations) {
            JSONObject aJSON = (JSONObject) asJSON(a);
            String aType = aJSON.keys().next();
            result.put(aType, aJSON.get(aType));
        }
        return result;
    }

    /**
     * Recursively serialize an Annotation or an Annotation field value to
     * a JSON compatible type.
     */
    private static Object asJSON(Object obj)
            throws IllegalAccessException, InvocationTargetException, JSONException {
        // Use instanceof to determine if it is an Annotation.
        // obj.getClass().isAnnotation() doesn't work as expected because
        // obj.getClass() returns a proxy class.
        if (obj instanceof Annotation) {
            Class<? extends Annotation> annotationType = ((Annotation) obj).annotationType();
            JSONObject json = new JSONObject();
            for (Method method : annotationType.getMethods()) {
                if (SKIP_METHODS.contains(method.getName())) {
                    continue;
                }
                json.put(method.getName(), asJSON(method.invoke(obj)));
            }
            JSONObject outerJson = new JSONObject();
            // If proguard is enabled and InnerClasses attribute is not kept,
            // then getCanonicalName() will return Outer$Inner instead of
            // Outer.Inner.  So just use getName().
            outerJson.put(annotationType.getName().replaceFirst(
                                  annotationType.getPackage().getName() + ".", ""),
                    json);
            return outerJson;
        } else {
            Class<?> clazz = obj.getClass();
            if (clazz.isArray()) {
                JSONArray jarr = new JSONArray();
                for (int i = 0; i < Array.getLength(obj); i++) {
                    jarr.put(asJSON(Array.get(obj, i)));
                }
                return jarr;
            } else {
                return obj;
            }
        }
    }
}
