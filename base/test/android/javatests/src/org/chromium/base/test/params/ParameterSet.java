// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import org.junit.Assert;

import java.io.File;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Callable;

/**
 * A set of parameters for one *SINGLE* test method or test class constructor.
 *
 * For example, <code>new ParameterSet().value("a", "b")</code> is intended for
 * a test method/constructor that takes in two string as arguments.
 * <code>public void testSimple(String a, String b) {...}</code>
 * or
 * <code>public MyTestClass(String a, String b) {...}</code>
 *
 * To parameterize testSimple or MyTestClass's tests, create multiple ParameterSets
 * <code>
 * static List<ParameterSet> sAllParameterSets = new ArrayList<>();
 * static {
 *   sAllParameterSets.add(new ParameterSet().value("a", "b");
 *   sAllParameterSets.add(new ParameterSet().value("c", "d");
 * }
 */
public class ParameterSet {
    private List<Object> mValues;
    private String mName;

    public ParameterSet() {}

    public ParameterSet value(Object firstArg, Object... objects) {
        List<Object> parameterList = new ArrayList<Object>();
        parameterList.add(firstArg);
        parameterList.addAll(Arrays.asList(objects));
        Assert.assertTrue(
                "Can not create ParameterSet with no parameters", parameterList.size() != 0);
        mValues = validateAndCopy(parameterList);
        return this;
    }

    public ParameterSet name(String name) {
        mName = name;
        return this;
    }

    @Override
    public String toString() {
        if (mValues == null) {
            return "null";
        }
        return Arrays.toString(mValues.toArray());
    }

    private List<Object> validateAndCopy(List<Object> values) {
        List<Object> tempValues = new ArrayList<>();
        for (Object o : values) {
            if (o == null) {
                tempValues.add(null);
            } else {
                if (o.getClass().isPrimitive()
                        || ACCEPTABLE_TYPES.contains(o.getClass())
                        || o instanceof Callable) {
                    tempValues.add(o);
                } else {
                    // TODO(yolandyan): maybe come up with way to support
                    // complex object while handling immutability at the
                    // same time
                    throw new IllegalArgumentException(
                            "Type \"%s\" is not supported in"
                                    + " parameterized testing at this time. Accepted types include"
                                    + " all primitive types along with "
                                    + Arrays.toString(
                                            ACCEPTABLE_TYPES.toArray(
                                                    new Class[ACCEPTABLE_TYPES.size()])));
                }
            }
        }
        return Collections.unmodifiableList(tempValues);
    }

    String getName() {
        if (mName == null) {
            return "";
        }
        return mName;
    }

    List<Object> getValues() {
        return mValues;
    }

    int size() {
        if (mValues == null) return 0;
        return mValues.size();
    }

    private static final Set<Class<?>> ACCEPTABLE_TYPES = getAcceptableTypes();

    /** Any immutable class is acceptable. */
    private static Set<Class<?>> getAcceptableTypes() {
        Set<Class<?>> ret = new HashSet<Class<?>>();
        ret.add(Boolean.class);
        ret.add(Byte.class);
        ret.add(Character.class);
        ret.add(Class.class);
        ret.add(Double.class);
        ret.add(File.class);
        ret.add(Float.class);
        ret.add(Integer.class);
        ret.add(Long.class);
        ret.add(Short.class);
        ret.add(String.class);
        ret.add(URL.class);
        ret.add(Void.class);
        return ret;
    }
}
