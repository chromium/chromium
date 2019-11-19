// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.support_lib_boundary.util;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.os.Build;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Arrays;
import java.util.Collection;

/**
 * A set of utility methods used for calling across the support library boundary.
 */
// Although this is not enforced in chromium, this is a requirement enforced when this file is
// mirrored into AndroidX. See http://b/120770118 for details.
@SuppressLint("BanTargetApiAnnotation")
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
     *
     * <p>A {@code null} {@link InvocationHandler} is treated as representing a {@code null} object.
     *
     * @param clazz a {@link Class} object representing the desired boundary interface.
     * @param invocationHandler an {@link InvocationHandler} compatible with this boundary
     *     interface.
     */
    @Nullable
    public static <T> T castToSuppLibClass(
            @NonNull Class<T> clazz, @Nullable InvocationHandler invocationHandler) {
        if (invocationHandler == null) return null;
        return clazz.cast(
                Proxy.newProxyInstance(BoundaryInterfaceReflectionUtil.class.getClassLoader(),
                        new Class[] {clazz}, invocationHandler));
    }

    /**
     * Create an {@link InvocationHandler} that delegates method calls to {@code delegate}, making
     * sure that the {@link Method} and parameters being passed exist in the same {@link
     * ClassLoader} as {@code delegate}.
     *
     * <p>A {@code null} delegate is represented with a {@code null} {@link InvocationHandler}.
     *
     * @param delegate the object which the resulting {@link InvocationHandler} should delegate
     *     method calls to.
     * @return an InvocationHandlerWithDelegateGetter wrapping {@code delegate}
     */
    @TargetApi(Build.VERSION_CODES.KITKAT)
    @Nullable
    public static InvocationHandler createInvocationHandlerFor(@Nullable final Object delegate) {
        if (delegate == null) return null;
        return new InvocationHandlerWithDelegateGetter(delegate);
    }

    /**
     * Plural version of {@link #createInvocationHandlerFor(Object)}. The resulting array will be
     * the same length as {@code delegates}, where the nth {@code InvocationHandler} wraps the nth
     * delegate object.
     *
     * <p>A {@code null} array of delegates is represented with a {@code null} array of {@link
     * InvocationHandler}s. Any individual {@code null} delegate is represented with a {@code null}
     * {@link InvocationHandler}.

     * @param delegates an array of objects to which to delegate.
     * @return an array of InvocationHandlerWithDelegateGetter instances, each delegating to
     *     the corresponding member of {@code delegates}.
     */
    @TargetApi(Build.VERSION_CODES.KITKAT)
    @Nullable
    public static InvocationHandler[] createInvocationHandlersForArray(
            @Nullable final Object[] delegates) {
        if (delegates == null) return null;

        InvocationHandler[] handlers = new InvocationHandler[delegates.length];
        for (int i = 0; i < handlers.length; i++) {
            handlers[i] = createInvocationHandlerFor(delegates[i]);
        }
        return handlers;
    }

    /**
     * Assuming that the given InvocationHandler was created in the current classloader and is an
     * InvocationHandlerWithDelegateGetter, return the object the InvocationHandler delegates its
     * method calls to.
     *
     * <p>A {@code null} {@link InvocationHandler} is treated as wrapping a {@code null} delegate.
     *
     * @param invocationHandler a {@link Nullable} InvocationHandlerWithDelegateGetter.
     * @return the corresponding delegate.
     */
    @Nullable
    public static Object getDelegateFromInvocationHandler(
            @Nullable InvocationHandler invocationHandler) {
        if (invocationHandler == null) return null;
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

        public InvocationHandlerWithDelegateGetter(@NonNull final Object delegate) {
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

        /**
         * Gets the delegate object (which is never {@code null}).
         */
        @NonNull
        public Object getDelegate() {
            return mDelegate;
        }
    }

    /**
     * Check if this is a debuggable build of Android. Note: we copy BuildInfo's method because we
     * cannot depend on the base-layer here (this folder is mirrored into Android).
     */
    private static boolean isDebuggable() {
        return "eng".equals(Build.TYPE) || "userdebug".equals(Build.TYPE);
    }

    /**
     * Check whether a set of features {@param features} contains a certain feature {@param
     * soughtFeature}.
     */
    public static boolean containsFeature(Collection<String> features, String soughtFeature) {
        assert !soughtFeature.endsWith(Features.DEV_SUFFIX);
        return features.contains(soughtFeature)
                || (isDebuggable() && features.contains(soughtFeature + Features.DEV_SUFFIX));
    }

    public static boolean containsFeature(String[] features, String soughtFeature) {
        return containsFeature(Arrays.asList(features), soughtFeature);
    }
}
