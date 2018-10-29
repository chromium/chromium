// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.support_lib_boundary.util;

import android.annotation.TargetApi;
import android.os.Build;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * A set of utility methods used for calling across the support library boundary.
 */
public class BoundaryInterfaceReflectionUtil {
    /**
     * Check if an object is an instance of {@code className}, resolving {@code className} in
     * the object's own ClassLoader. This is useful when {@code obj} may have been created in a
     * ClassLoader other than the current one (in which case {@code obj instanceof Foo} would fail
     * but {@code instanceOfInOwnClassLoader(obj, "Foo")} may succeed).
     */
    public static boolean instanceOfInOwnClassLoader(Object obj, String className) {
        try {
            ClassLoader loader = obj.getClass().getClassLoader();
            // We intentionally set initialize = false because instanceof shouldn't trigger
            // static initialization.
            Class<?> clazz = Class.forName(className, false, loader);
            return clazz.isInstance(obj);
        } catch (ClassNotFoundException e) {
            // If className is not in the ClassLoader, then this cannot be an instance.
            return false;
        }
    }

    /**
     * Utility method for fetching a method from {@param delegateLoader}, with the same signature
     * (package + class + method name + parameters) as a given method defined in another
     * classloader.
     */
    public static Method dupeMethod(Method method, ClassLoader delegateLoader)
            throws ClassNotFoundException, NoSuchMethodException {
        // We're converting one type to another. This is analogous to instantiating the type on the
        // other side of the Boundary, so it makes sense to perform static initialization if it
        // hasn't already happened (initialize = true).
        Class<?> declaringClass =
                Class.forName(method.getDeclaringClass().getName(), true, delegateLoader);
        // We do not need to convert parameter types across ClassLoaders because we never pass
        // BoundaryInterfaces in methods, but pass InvocationHandlers instead.
        Class[] parameterClasses = method.getParameterTypes();
        return declaringClass.getDeclaredMethod(method.getName(), parameterClasses);
    }

    /**
     * Returns an implementation of the boundary interface named clazz, by delegating method calls
     * to the {@link InvocationHandler} invocationHandler.
     */
    public static <T> T castToSuppLibClass(Class<T> clazz, InvocationHandler invocationHandler) {
        return clazz.cast(
                Proxy.newProxyInstance(BoundaryInterfaceReflectionUtil.class.getClassLoader(),
                        new Class[] {clazz}, invocationHandler));
    }

    /**
     * Create an {@link java.lang.reflect.InvocationHandler} that delegates method calls to
     * {@param delegate}, making sure that the {@link java.lang.reflect.Method} and parameters being
     * passed to {@param delegate} exist in the same {@link java.lang.ClassLoader} as {@param
     * delegate}.
     */
    @TargetApi(Build.VERSION_CODES.KITKAT)
    public static InvocationHandler createInvocationHandlerFor(final Object delegate) {
        return new InvocationHandlerWithDelegateGetter(delegate);
    }

    /**
     * Assuming that the given InvocationHandler was created in the current classloader and is an
     * InvocationHandlerWithDelegateGetter, return the object the InvocationHandler delegates its
     * method calls to.
     */
    public static Object getDelegateFromInvocationHandler(InvocationHandler invocationHandler) {
        InvocationHandlerWithDelegateGetter objectHolder =
                (InvocationHandlerWithDelegateGetter) invocationHandler;
        return objectHolder.getDelegate();
    }

    /**
     * An InvocationHandler storing the original object that method calls are delegated to.
     * This allows us to pass InvocationHandlers across the support library boundary and later
     * unwrap the objects used as delegates within those InvocationHandlers.
     */
    @TargetApi(Build.VERSION_CODES.KITKAT)
    private static class InvocationHandlerWithDelegateGetter implements InvocationHandler {
        private final Object mDelegate;

        public InvocationHandlerWithDelegateGetter(final Object delegate) {
            mDelegate = delegate;
        }

        @Override
        public Object invoke(Object o, Method method, Object[] objects) throws Throwable {
            final ClassLoader delegateLoader = mDelegate.getClass().getClassLoader();
            try {
                return dupeMethod(method, delegateLoader).invoke(mDelegate, objects);
            } catch (InvocationTargetException e) {
                // If something went wrong, ensure we throw the original exception.
                throw e.getTargetException();
            } catch (ReflectiveOperationException e) {
                throw new RuntimeException("Reflection failed for method " + method, e);
            }
        }

        public Object getDelegate() {
            return mDelegate;
        }
    }

    /**
     * Check whether a set of features {@param features} contains a certain feature {@param
     * soughtFeature}.
     */
    public static boolean containsFeature(String[] features, String soughtFeature) {
        for (String feature : features) {
            if (feature.equals(soughtFeature)) return true;
        }
        return false;
    }
}
