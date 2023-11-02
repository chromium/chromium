// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.Type;
import org.objectweb.asm.commons.MethodRemapper;
import org.objectweb.asm.commons.Remapper;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Method;

/**
 * Java application that modifies Fragment.getActivity() to return an Activity instead of a
 * FragmentActivity, and updates any existing getActivity() calls to reference the updated method.
 *
 * See crbug.com/1144345 for more context.
 */
public class FragmentActivityReplacer extends ByteCodeRewriter {
    private static final String GET_ACTIVITY_METHOD_NAME = "getActivity";
    private static final String GET_LIFECYCLE_ACTIVITY_METHOD_NAME = "getLifecycleActivity";
    private static final String NEW_METHOD_DESCRIPTOR = "()Landroid/app/Activity;";
    private static final String OLD_METHOD_DESCRIPTOR =
            "()Landroidx/fragment/app/FragmentActivity;";
    private static final String REQUIRE_ACTIVITY_METHOD_NAME = "requireActivity";
    private static final String SUPPORT_LIFECYCLE_FRAGMENT_IMPL_BINARY_NAME =
            "com.google.android.gms.common.api.internal.SupportLifecycleFragmentImpl";

    public static void main(String[] args) throws IOException {
        // Invoke this script using //build/android/gyp/bytecode_rewriter.py
        if (!(args.length == 2 || args.length == 3 && args[0].equals("--single-androidx"))) {
            System.err.println("Expected arguments: [--single-androidx] <input.jar> <output.jar>");
            System.exit(1);
        }

        if (args.length == 2) {
            FragmentActivityReplacer rewriter = new FragmentActivityReplacer(false);
            rewriter.rewrite(new File(args[0]), new File(args[1]));
        } else {
            FragmentActivityReplacer rewriter = new FragmentActivityReplacer(true);
            rewriter.rewrite(new File(args[1]), new File(args[2]));
        }
    }

    private final boolean mSingleAndroidX;

    public FragmentActivityReplacer(boolean singleAndroidX) {
        mSingleAndroidX = singleAndroidX;
    }

    @Override
    protected boolean shouldRewriteClass(String classPath) {
        return true;
    }

    @Override
    protected ClassVisitor getClassVisitorForClass(String classPath, ClassVisitor delegate) {
        ClassVisitor invocationVisitor = new InvocationReplacer(delegate, mSingleAndroidX);
        switch (classPath) {
            case "androidx/fragment/app/Fragment.class":
                return new FragmentClassVisitor(invocationVisitor);
            case "com/google/android/gms/common/api/internal/SupportLifecycleFragmentImpl.class":
                return new SupportLifecycleFragmentImplClassVisitor(invocationVisitor);
            default:
                return invocationVisitor;
        }
    }

    /**
     * Updates any Fragment.getActivity/requireActivity() or getLifecycleActivity() calls to call
     * the replaced method.
     */
    private static class InvocationReplacer extends ClassVisitor {
        /**
         * A ClassLoader that will resolve R classes to Object.
         *
         * R won't be in our classpath, and we don't access any information about them, so resolving
         * it to a dummy value is fine.
         */
        private static class ResourceStubbingClassLoader extends ClassLoader {
            @Override
            protected Class<?> findClass(String name) throws ClassNotFoundException {
                if (name.matches(".*\\.R(\\$.+)?")) {
                    return Object.class;
                }
                return super.findClass(name);
            }
        }

        private final boolean mSingleAndroidX;
        private final ClassLoader mClassLoader;

        private InvocationReplacer(ClassVisitor baseVisitor, boolean singleAndroidX) {
            super(Opcodes.ASM7, baseVisitor);
            mSingleAndroidX = singleAndroidX;
            mClassLoader = new ResourceStubbingClassLoader();
        }

        @Override
        public MethodVisitor visitMethod(
                int access, String name, String descriptor, String signature, String[] exceptions) {
            MethodVisitor base = super.visitMethod(access, name, descriptor, signature, exceptions);
            return new MethodVisitor(Opcodes.ASM7, base) {
                @Override
                public void visitMethodInsn(int opcode, String owner, String name,
                        String descriptor, boolean isInterface) {
                    // Change the return type of getActivity and replaceActivity.
                    if (isActivityGetterInvocation(opcode, owner, name, descriptor)) {
                        super.visitMethodInsn(
                                opcode, owner, name, NEW_METHOD_DESCRIPTOR, isInterface);
                        if (mSingleAndroidX) {
                            super.visitTypeInsn(
                                    Opcodes.CHECKCAST, "androidx/fragment/app/FragmentActivity");
                        }
                    } else if (isDowncastableFragmentActivityMethodInvocation(
                                       opcode, owner, name, descriptor)) {
                        // Replace FragmentActivity.foo() with Activity.foo() to fix cases where the
                        // above code changed the getActivity return type. See the
                        // isDowncastableFragmentActivityMethodInvocation documentation for details.
                        super.visitMethodInsn(
                                opcode, "android/app/Activity", name, descriptor, isInterface);
                    } else {
                        super.visitMethodInsn(opcode, owner, name, descriptor, isInterface);
                    }
                }

                private boolean isActivityGetterInvocation(
                        int opcode, String owner, String name, String descriptor) {
                    boolean isFragmentGetActivity = name.equals(GET_ACTIVITY_METHOD_NAME)
                            && descriptor.equals(OLD_METHOD_DESCRIPTOR)
                            && isFragmentSubclass(owner);
                    boolean isFragmentRequireActivity = name.equals(REQUIRE_ACTIVITY_METHOD_NAME)
                            && descriptor.equals(OLD_METHOD_DESCRIPTOR)
                            && isFragmentSubclass(owner);
                    boolean isSupportLifecycleFragmentImplGetLifecycleActivity =
                            name.equals(GET_LIFECYCLE_ACTIVITY_METHOD_NAME)
                            && descriptor.equals(OLD_METHOD_DESCRIPTOR)
                            && owner.equals(SUPPORT_LIFECYCLE_FRAGMENT_IMPL_BINARY_NAME);
                    return (opcode == Opcodes.INVOKEVIRTUAL || opcode == Opcodes.INVOKESPECIAL)
                            && (isFragmentGetActivity || isFragmentRequireActivity
                                    || isSupportLifecycleFragmentImplGetLifecycleActivity);
                }

                /**
                 * Returns true if the given method belongs to FragmentActivity, and also exists on
                 * Activity.
                 *
                 * The Java code `requireActivity().getClassLoader()` will compile to the following
                 * bytecode:
                 *   aload_0
                 *   // Method requireActivity:()Landroid/app/Activity;
                 *   invokevirtual #n
                 *   // Method androidx/fragment/app/FragmentActivity.getClassLoader:()LClassLoader;
                 *   invokevirtual #m
                 *
                 * The second invokevirtual instruction doesn't typecheck because the
                 * requireActivity() return type was changed from FragmentActivity to Activity. Note
                 * that this is only an issue when validating the bytecode on the JVM, not in
                 * Dalvik, so while the above code works on device, it fails in robolectric tests.
                 *
                 * To fix the example above, we'd replace the second invokevirtual call with a call
                 * to android/app/Activity.getClassLoader:()Ljava/lang/ClassLoader. In general, any
                 * call to FragmentActivity.foo, where foo also exists on Activity, will be replaced
                 * with a call to Activity.foo. Activity.foo will still resolve to
                 * FragmentActivity.foo at runtime, while typechecking in robolectric tests.
                 */
                private boolean isDowncastableFragmentActivityMethodInvocation(
                        int opcode, String owner, String name, String descriptor) {
                    // Return if this isn't an invoke instruction on a FragmentActivity.
                    if (!(opcode == Opcodes.INVOKEVIRTUAL || opcode == Opcodes.INVOKESPECIAL)
                            || !owner.equals("androidx/fragment/app/FragmentActivity")) {
                        return false;
                    }
                    try {
                        // Check if the method exists in Activity.
                        Class<?> activity = mClassLoader.loadClass("android.app.Activity");
                        for (Method activityMethod : activity.getMethods()) {
                            if (activityMethod.getName().equals(name)
                                    && Type.getMethodDescriptor(activityMethod)
                                               .equals(descriptor)) {
                                return true;
                            }
                        }
                        return false;
                    } catch (ClassNotFoundException e) {
                        throw new RuntimeException(e);
                    }
                }

                private boolean isFragmentSubclass(String internalType) {
                    // This doesn't use Class#isAssignableFrom to avoid us needing to load
                    // AndroidX's Fragment class, which may not be on the classpath.
                    try {
                        String binaryName = Type.getObjectType(internalType).getClassName();
                        Class<?> clazz = mClassLoader.loadClass(binaryName);
                        while (clazz != null) {
                            if (clazz.getName().equals("androidx.fragment.app.Fragment")) {
                                return true;
                            }
                            clazz = clazz.getSuperclass();
                        }
                        return false;
                    } catch (ClassNotFoundException e) {
                        throw new RuntimeException(e);
                    }
                }
            };
        }
    }

    /**
     * Updates the implementation of Fragment.getActivity() and Fragment.requireActivity().
     */
    private static class FragmentClassVisitor extends ClassVisitor {
        private FragmentClassVisitor(ClassVisitor baseVisitor) {
            super(Opcodes.ASM7, baseVisitor);
        }

        @Override
        public MethodVisitor visitMethod(
                int access, String name, String descriptor, String signature, String[] exceptions) {
            // Update the descriptor of getActivity() and requireActivity().
            MethodVisitor baseVisitor;
            if (descriptor.equals(OLD_METHOD_DESCRIPTOR)
                    && (name.equals(GET_ACTIVITY_METHOD_NAME)
                            || name.equals(REQUIRE_ACTIVITY_METHOD_NAME))) {
                // Some Fragments in a Clank library implement an interface that defines an
                // `Activity getActivity()` method. Fragment.getActivity() is considered its
                // implementation from a typechecking perspective, but javac still generates a
                // getActivity() method in these Fragments that call Fragment.getActivity(). This
                // isn't an issue when the methods return different types, but after changing
                // Fragment.getActivity() to return an Activity, this generated implementation is
                // now overriding Fragment's, which it can't do because Fragment.getActivity() is
                // final. We make it non-final here to avoid this issue.
                baseVisitor = super.visitMethod(
                        access & ~Opcodes.ACC_FINAL, name, NEW_METHOD_DESCRIPTOR, null, exceptions);
            } else {
                baseVisitor = super.visitMethod(access, name, descriptor, signature, exceptions);
            }

            // Replace getActivity() with `return ContextUtils.activityFromContext(getContext());`
            if (name.equals(GET_ACTIVITY_METHOD_NAME) && descriptor.equals(OLD_METHOD_DESCRIPTOR)) {
                baseVisitor.visitVarInsn(Opcodes.ALOAD, 0);
                baseVisitor.visitMethodInsn(Opcodes.INVOKEVIRTUAL, "androidx/fragment/app/Fragment",
                        "getContext", "()Landroid/content/Context;", false);
                baseVisitor.visitMethodInsn(Opcodes.INVOKESTATIC, "org/chromium/utils/ContextUtils",
                        "activityFromContext", "(Landroid/content/Context;)Landroid/app/Activity;",
                        false);
                baseVisitor.visitInsn(Opcodes.ARETURN);
                // Since we set COMPUTE_FRAMES, the arguments of visitMaxs are ignored, but calling
                // it forces ClassWriter to actually recompute the correct stack/local values.
                // Without this call ClassWriter keeps the original stack=0,locals=1 which is wrong.
                baseVisitor.visitMaxs(0, 0);
                return null;
            }

            return new MethodRemapper(baseVisitor, new Remapper() {
                @Override
                public String mapType(String internalName) {
                    if (internalName.equals("androidx/fragment/app/FragmentActivity")) {
                        return "android/app/Activity";
                    }
                    return internalName;
                }
            });
        }
    }

    /**
     * Update SupportLifecycleFragmentImpl.getLifecycleActivity().
     */
    private static class SupportLifecycleFragmentImplClassVisitor extends ClassVisitor {
        private SupportLifecycleFragmentImplClassVisitor(ClassVisitor baseVisitor) {
            super(Opcodes.ASM7, baseVisitor);
        }

        @Override
        public MethodVisitor visitMethod(
                int access, String name, String descriptor, String signature, String[] exceptions) {
            // SupportLifecycleFragmentImpl has two getActivity methods:
            //   1. public FragmentActivity getLifecycleActivity():
            //      This is what you'll see in the source. This delegates to Fragment.getActivity().
            //   2. public Activity getLifecycleActivity():
            //      This is generated because the class implements LifecycleFragment, which
            //      declares this method, and delegates to #1.
            //
            // Here we change the return type of #1 and delete #2.
            if (name.equals(GET_LIFECYCLE_ACTIVITY_METHOD_NAME)) {
                if (descriptor.equals(OLD_METHOD_DESCRIPTOR)) {
                    return super.visitMethod(
                            access, name, NEW_METHOD_DESCRIPTOR, signature, exceptions);
                }
                return null;
            }
            return super.visitMethod(access, name, descriptor, signature, exceptions);
        }
    }
}
