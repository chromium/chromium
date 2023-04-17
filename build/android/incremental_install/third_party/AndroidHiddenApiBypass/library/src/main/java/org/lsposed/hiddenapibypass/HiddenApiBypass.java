/*
 * Copyright (C) 2021 LSPosed
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.lsposed.hiddenapibypass;

import android.os.Build;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.lsposed.hiddenapibypass.library.BuildConfig;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandleInfo;
import java.lang.invoke.MethodHandles;
import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Type;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

import dalvik.system.VMRuntime;
import sun.misc.Unsafe;

@RequiresApi(Build.VERSION_CODES.P)
public final class HiddenApiBypass {
    private static final String TAG = "HiddenApiBypass";
    private static final Unsafe unsafe;
    private static final long methodOffset;
    private static final long classOffset;
    private static final long artOffset;
    private static final long infoOffset;
    private static final long methodsOffset;
    private static final long iFieldOffset;
    private static final long sFieldOffset;
    private static final long memberOffset;
    private static final long artMethodSize;
    private static final long artMethodBias;
    private static final long artFieldSize;
    private static final long artFieldBias;
    private static final Set<String> signaturePrefixes = new HashSet<>();

    static {
        try {
            //noinspection JavaReflectionMemberAccess DiscouragedPrivateApi
            unsafe = (Unsafe) Unsafe.class.getDeclaredMethod("getUnsafe").invoke(null);
            assert unsafe != null;
            methodOffset = unsafe.objectFieldOffset(Helper.Executable.class.getDeclaredField("artMethod"));
            classOffset = unsafe.objectFieldOffset(Helper.Executable.class.getDeclaredField("declaringClass"));
            artOffset = unsafe.objectFieldOffset(Helper.MethodHandle.class.getDeclaredField("artFieldOrMethod"));
            infoOffset = unsafe.objectFieldOffset(Helper.MethodHandleImpl.class.getDeclaredField("info"));
            methodsOffset = unsafe.objectFieldOffset(Helper.Class.class.getDeclaredField("methods"));
            iFieldOffset = unsafe.objectFieldOffset(Helper.Class.class.getDeclaredField("iFields"));
            sFieldOffset = unsafe.objectFieldOffset(Helper.Class.class.getDeclaredField("sFields"));
            memberOffset = unsafe.objectFieldOffset(Helper.HandleInfo.class.getDeclaredField("member"));
            Method mA = Helper.NeverCall.class.getDeclaredMethod("a");
            Method mB = Helper.NeverCall.class.getDeclaredMethod("b");
            mA.setAccessible(true);
            mB.setAccessible(true);
            MethodHandle mhA = MethodHandles.lookup().unreflect(mA);
            MethodHandle mhB = MethodHandles.lookup().unreflect(mB);
            long aAddr = unsafe.getLong(mhA, artOffset);
            long bAddr = unsafe.getLong(mhB, artOffset);
            long aMethods = unsafe.getLong(Helper.NeverCall.class, methodsOffset);
            artMethodSize = bAddr - aAddr;
            if (BuildConfig.DEBUG) Log.v(TAG, artMethodSize + " " +
                    Long.toString(aAddr, 16) + ", " +
                    Long.toString(bAddr, 16) + ", " +
                    Long.toString(aMethods, 16));
            artMethodBias = aAddr - aMethods - artMethodSize;
            Field fI = Helper.NeverCall.class.getDeclaredField("i");
            Field fJ = Helper.NeverCall.class.getDeclaredField("j");
            fI.setAccessible(true);
            fJ.setAccessible(true);
            MethodHandle mhI = MethodHandles.lookup().unreflectGetter(fI);
            MethodHandle mhJ = MethodHandles.lookup().unreflectGetter(fJ);
            long iAddr = unsafe.getLong(mhI, artOffset);
            long jAddr = unsafe.getLong(mhJ, artOffset);
            long iFields = unsafe.getLong(Helper.NeverCall.class, iFieldOffset);
            artFieldSize = jAddr - iAddr;
            if (BuildConfig.DEBUG) Log.v(TAG, artFieldSize + " " +
                    Long.toString(iAddr, 16) + ", " +
                    Long.toString(jAddr, 16) + ", " +
                    Long.toString(iFields, 16));
            artFieldBias = iAddr - iFields;
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Initialize error", e);
            throw new ExceptionInInitializerError(e);
        }
    }

    @VisibleForTesting
    static boolean checkArgsForInvokeMethod(Class<?>[] params, Object[] args) {
        if (params.length != args.length) return false;
        for (int i = 0; i < params.length; ++i) {
            if (params[i].isPrimitive()) {
                if (params[i] == int.class && !(args[i] instanceof Integer)) return false;
                else if (params[i] == byte.class && !(args[i] instanceof Byte)) return false;
                else if (params[i] == char.class && !(args[i] instanceof Character)) return false;
                else if (params[i] == boolean.class && !(args[i] instanceof Boolean)) return false;
                else if (params[i] == double.class && !(args[i] instanceof Double)) return false;
                else if (params[i] == float.class && !(args[i] instanceof Float)) return false;
                else if (params[i] == long.class && !(args[i] instanceof Long)) return false;
                else if (params[i] == short.class && !(args[i] instanceof Short)) return false;
            } else if (args[i] != null && !params[i].isInstance(args[i])) return false;
        }
        return true;
    }

    /**
     * create an instance of the given class {@code clazz} calling the restricted constructor with arguments {@code args}
     *
     * @param clazz    the class of the instance to new
     * @param initargs arguments to call constructor
     * @return the new instance
     * @see Constructor#newInstance(Object...)
     */
    public static Object newInstance(@NonNull Class<?> clazz, Object... initargs) throws NoSuchMethodException, IllegalAccessException, InvocationTargetException, InstantiationException {
        Method stub = Helper.InvokeStub.class.getDeclaredMethod("invoke", Object[].class);
        Constructor<?> ctor = Helper.InvokeStub.class.getDeclaredConstructor(Object[].class);
        ctor.setAccessible(true);
        long methods = unsafe.getLong(clazz, methodsOffset);
        if (methods == 0) throw new NoSuchMethodException("Cannot find matching constructor");
        int numMethods = unsafe.getInt(methods);
        if (BuildConfig.DEBUG) Log.d(TAG, clazz + " has " + numMethods + " methods");
        for (int i = 0; i < numMethods; i++) {
            long method = methods + i * artMethodSize + artMethodBias;
            unsafe.putLong(stub, methodOffset, method);
            if (BuildConfig.DEBUG) Log.v(TAG, "got " + clazz.getTypeName() + "." + stub.getName() +
                    "(" + Arrays.stream(stub.getParameterTypes()).map(Type::getTypeName).collect(Collectors.joining()) + ")");
            if ("<init>".equals(stub.getName())) {
                unsafe.putLong(ctor, methodOffset, method);
                unsafe.putObject(ctor, classOffset, clazz);
                Class<?>[] params = ctor.getParameterTypes();
                if (checkArgsForInvokeMethod(params, initargs))
                    return ctor.newInstance(initargs);
            }
        }
        throw new NoSuchMethodException("Cannot find matching constructor");
    }

    /**
     * invoke a restrict method named {@code methodName} of the given class {@code clazz} with this object {@code thiz} and arguments {@code args}
     *
     * @param clazz      the class call the method on (this parameter is required because this method cannot call inherit method)
     * @param thiz       this object, which can be {@code null} if the target method is static
     * @param methodName the method name
     * @param args       arguments to call the method with name {@code methodName}
     * @return the return value of the method
     * @see Method#invoke(Object, Object...)
     */
    public static Object invoke(@NonNull Class<?> clazz, @Nullable Object thiz, @NonNull String methodName, Object... args) throws NoSuchMethodException, InvocationTargetException, IllegalAccessException {
        if (thiz != null && !clazz.isInstance(thiz)) {
            throw new IllegalArgumentException("this object is not an instance of the given class");
        }
        Method stub = Helper.InvokeStub.class.getDeclaredMethod("invoke", Object[].class);
        stub.setAccessible(true);
        long methods = unsafe.getLong(clazz, methodsOffset);
        if (methods == 0) throw new NoSuchMethodException("Cannot find matching method");
        int numMethods = unsafe.getInt(methods);
        if (BuildConfig.DEBUG) Log.d(TAG, clazz + " has " + numMethods + " methods");
        for (int i = 0; i < numMethods; i++) {
            long method = methods + i * artMethodSize + artMethodBias;
            unsafe.putLong(stub, methodOffset, method);
            if (BuildConfig.DEBUG) Log.v(TAG, "got " + clazz.getTypeName() + "." + stub.getName() +
                    "(" + Arrays.stream(stub.getParameterTypes()).map(Type::getTypeName).collect(Collectors.joining()) + ")");
            if (methodName.equals(stub.getName())) {
                Class<?>[] params = stub.getParameterTypes();
                if (checkArgsForInvokeMethod(params, args))
                    return stub.invoke(thiz, args);
            }
        }
        throw new NoSuchMethodException("Cannot find matching method");
    }

    /**
     * get declared methods of given class without hidden api restriction
     *
     * @param clazz the class to fetch declared methods (including constructors with name `&lt;init&gt;`)
     * @return list of declared methods of {@code clazz}
     */
    @NonNull
    public static List<Executable> getDeclaredMethods(@NonNull Class<?> clazz) {
        ArrayList<Executable> list = new ArrayList<>();
        if (clazz.isPrimitive() || clazz.isArray()) return list;
        MethodHandle mh;
        try {
            Method mA = Helper.NeverCall.class.getDeclaredMethod("a");
            mA.setAccessible(true);
            mh = MethodHandles.lookup().unreflect(mA);
        } catch (NoSuchMethodException | IllegalAccessException e) {
            return list;
        }
        long methods = unsafe.getLong(clazz, methodsOffset);
        if (methods == 0) return list;
        int numMethods = unsafe.getInt(methods);
        if (BuildConfig.DEBUG) Log.d(TAG, clazz + " has " + numMethods + " methods");
        for (int i = 0; i < numMethods; i++) {
            long method = methods + i * artMethodSize + artMethodBias;
            unsafe.putLong(mh, artOffset, method);
            unsafe.putObject(mh, infoOffset, null);
            try {
                MethodHandles.lookup().revealDirect(mh);
            } catch (Throwable ignored) {
            }
            MethodHandleInfo info = (MethodHandleInfo) unsafe.getObject(mh, infoOffset);
            Executable member = (Executable) unsafe.getObject(info, memberOffset);
            if (BuildConfig.DEBUG)
                Log.v(TAG, "got " + clazz.getTypeName() + "." + member.getName() +
                        "(" + Arrays.stream(member.getParameterTypes()).map(Type::getTypeName).collect(Collectors.joining()) + ")");
            list.add(member);
        }
        return list;
    }

    /**
     * get a restrict method named {@code methodName} of the given class {@code clazz} with argument types {@code parameterTypes}
     *
     * @param clazz          the class where the expected method declares
     * @param methodName     the expected method's name
     * @param parameterTypes argument types of the expected method with name {@code methodName}
     * @return the found method
     * @throws NoSuchMethodException when no method matches the given parameters
     * @see Class#getDeclaredMethod(String, Class[]) 
     */
    @NonNull
    public static Method getDeclaredMethod(@NonNull Class<?> clazz, @NonNull String methodName, @NonNull Class<?>... parameterTypes) throws NoSuchMethodException {
        List<Executable> methods = getDeclaredMethods(clazz);
        allMethods:
        for (Executable method : methods) {
            if (!method.getName().equals(methodName)) continue;
            if (!(method instanceof Method)) continue;
            Class<?>[] expectedTypes = method.getParameterTypes();
            if (expectedTypes.length != parameterTypes.length) continue;
            for (int i = 0; i < parameterTypes.length; ++i) {
                if (parameterTypes[i] != expectedTypes[i]) continue allMethods;
            }
            return (Method) method;
        }
        throw new NoSuchMethodException("Cannot find matching method");
    }

    /**
     * get a restrict constructor of the given class {@code clazz} with argument types {@code parameterTypes}
     *
     * @param clazz          the class where the expected constructor declares
     * @param parameterTypes argument types of the expected constructor
     * @return the found constructor
     * @throws NoSuchMethodException when no constructor matches the given parameters
     * @see Class#getDeclaredConstructor(Class[])
     */
    @NonNull
    public static Constructor<?> getDeclaredConstructor(@NonNull Class<?> clazz, @NonNull Class<?>... parameterTypes) throws NoSuchMethodException {
        List<Executable> methods = getDeclaredMethods(clazz);
        allMethods:
        for (Executable method : methods) {
            if (!(method instanceof Constructor)) continue;
            Class<?>[] expectedTypes = method.getParameterTypes();
            if (expectedTypes.length != parameterTypes.length) continue;
            for (int i = 0; i < parameterTypes.length; ++i) {
                if (parameterTypes[i] != expectedTypes[i]) continue allMethods;
            }
            return (Constructor<?>) method;
        }
        throw new NoSuchMethodException("Cannot find matching constructor");
    }


    /**
     * get declared non-static fields of given class without hidden api restriction
     *
     * @param clazz the class to fetch declared methods
     * @return list of declared non-static fields of {@code clazz}
     */
    @NonNull
    public static List<Field> getInstanceFields(@NonNull Class<?> clazz) {
        ArrayList<Field> list = new ArrayList<>();
        if (clazz.isPrimitive() || clazz.isArray()) return list;
        MethodHandle mh;
        try {
            Field fI = Helper.NeverCall.class.getDeclaredField("i");
            fI.setAccessible(true);
            mh = MethodHandles.lookup().unreflectGetter(fI);
        } catch (IllegalAccessException | NoSuchFieldException e) {
            return list;
        }
        long fields = unsafe.getLong(clazz, iFieldOffset);
        if (fields == 0) return list;
        int numFields = unsafe.getInt(fields);
        if (BuildConfig.DEBUG) Log.d(TAG, clazz + " has " + numFields + " instance fields");
        for (int i = 0; i < numFields; i++) {
            long field = fields + i * artFieldSize + artFieldBias;
            unsafe.putLong(mh, artOffset, field);
            unsafe.putObject(mh, infoOffset, null);
            try {
                MethodHandles.lookup().revealDirect(mh);
            } catch (Throwable ignored) {
            }
            MethodHandleInfo info = (MethodHandleInfo) unsafe.getObject(mh, infoOffset);
            Field member = (Field) unsafe.getObject(info, memberOffset);
            if (BuildConfig.DEBUG)
                Log.v(TAG, "got " + member.getType() + " " + clazz.getTypeName() + "." + member.getName());
            list.add(member);
        }
        return list;
    }

    /**
     * get declared static fields of given class without hidden api restriction
     *
     * @param clazz the class to fetch declared methods
     * @return list of declared static fields of {@code clazz}
     */
    @NonNull
    public static List<Field> getStaticFields(@NonNull Class<?> clazz) {
        ArrayList<Field> list = new ArrayList<>();
        if (clazz.isPrimitive() || clazz.isArray()) return list;
        MethodHandle mh;
        try {
            Field fS = Helper.NeverCall.class.getDeclaredField("s");
            fS.setAccessible(true);
            mh = MethodHandles.lookup().unreflectGetter(fS);
        } catch (IllegalAccessException | NoSuchFieldException e) {
            return list;
        }
        long fields = unsafe.getLong(clazz, sFieldOffset);
        if (fields == 0) return list;
        int numFields = unsafe.getInt(fields);
        if (BuildConfig.DEBUG) Log.d(TAG, clazz + " has " + numFields + " static fields");
        for (int i = 0; i < numFields; i++) {
            long field = fields + i * artFieldSize + artFieldBias;
            unsafe.putLong(mh, artOffset, field);
            unsafe.putObject(mh, infoOffset, null);
            try {
                MethodHandles.lookup().revealDirect(mh);
            } catch (Throwable ignored) {
            }
            MethodHandleInfo info = (MethodHandleInfo) unsafe.getObject(mh, infoOffset);
            Field member = (Field) unsafe.getObject(info, memberOffset);
            if (BuildConfig.DEBUG)
                Log.v(TAG, "got " + member.getType() + " " + clazz.getTypeName() + "." + member.getName());
            list.add(member);
        }
        return list;
    }

    /**
     * Sets the list of exemptions from hidden API access enforcement.
     *
     * @param signaturePrefixes A list of class signature prefixes. Each item in the list is a prefix match on the type
     *                          signature of a blacklisted API. All matching APIs are treated as if they were on
     *                          the whitelist: access permitted, and no logging..
     * @return whether the operation is successful
     */
    public static boolean setHiddenApiExemptions(@NonNull String... signaturePrefixes) {
        try {
            Object runtime = invoke(VMRuntime.class, null, "getRuntime");
            invoke(VMRuntime.class, runtime, "setHiddenApiExemptions", (Object) signaturePrefixes);
            return true;
        } catch (Throwable e) {
            Log.w(TAG, "setHiddenApiExemptions", e);
            return false;
        }
    }

    /**
     * Adds the list of exemptions from hidden API access enforcement.
     *
     * @param signaturePrefixes A list of class signature prefixes. Each item in the list is a prefix match on the type
     *                          signature of a blacklisted API. All matching APIs are treated as if they were on
     *                          the whitelist: access permitted, and no logging..
     * @return whether the operation is successful
     */
    public static boolean addHiddenApiExemptions(String... signaturePrefixes) {
        HiddenApiBypass.signaturePrefixes.addAll(Arrays.asList(signaturePrefixes));
        String[] strings = new String[HiddenApiBypass.signaturePrefixes.size()];
        HiddenApiBypass.signaturePrefixes.toArray(strings);
        return setHiddenApiExemptions(strings);
    }

    /**
     * Clear the list of exemptions from hidden API access enforcement.
     * Android runtime will cache access flags, so if a hidden API has been accessed unrestrictedly,
     * running this method will not restore the restriction on it.
     *
     * @return whether the operation is successful
     */
    public static boolean clearHiddenApiExemptions() {
        HiddenApiBypass.signaturePrefixes.clear();
        return setHiddenApiExemptions();
    }
}
