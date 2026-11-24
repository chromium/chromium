// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import static org.objectweb.asm.ClassReader.EXPAND_FRAMES;
import static org.objectweb.asm.Opcodes.ACC_ABSTRACT;
import static org.objectweb.asm.Opcodes.ACC_INTERFACE;
import static org.objectweb.asm.Opcodes.ASM9;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.MethodVisitor;

import java.util.ArrayList;

/**
 * This ClassVisitor verifies that a class and its methods are suitable for rewriting. Given a class
 * and a list of methods it performs the following checks: 1. Class is subclass of a class that we
 * want to trace. 2. Class is not abstract or an interface.
 *
 * <p>For each method provided in {@code methodsToCheck}: If the class overrides the method then we
 * can rewrite it directly. If the class doesn't override the method then we can generate an
 * override with {@link EmptyOverrideGeneratorClassAdapter}, but first we must check if the parent
 * method is private or final using {@link ParentMethodCheckerClassAdapter}.
 *
 * <p>This adapter modifies the provided method list to indicate which methods should be overridden
 * or skipped.
 */
class MethodCheckerClassAdapter extends ClassVisitor {
    private static final String VIEW_CLASS_DESCRIPTOR = "android/view/View";
    private static final String ANIMATOR_UPDATE_LISTENER_CLASS_DESCRIPTOR =
            "android/animation/ValueAnimator$AnimatorUpdateListener";
    private static final String ANIMATOR_LISTENER_CLASS_DESCRIPTOR =
            "android/animation/Animator$AnimatorListener";

    private final ArrayList<MethodDescription> mMethodsToCheck;
    private final ClassHierarchy mClassHierarchy;
    private String mSuperName;

    public MethodCheckerClassAdapter(
            ArrayList<MethodDescription> methodsToCheck, ClassHierarchy classHierarchy) {
        super(ASM9);
        mMethodsToCheck = methodsToCheck;
        mClassHierarchy = classHierarchy;
    }

    @Override
    public void visit(
            int version,
            int access,
            String name,
            String signature,
            String superName,
            String[] interfaces) {
        super.visit(version, access, name, signature, superName, interfaces);

        mSuperName = superName;

        boolean isAbstract = (access & ACC_ABSTRACT) == ACC_ABSTRACT;
        boolean isInterface = (access & ACC_INTERFACE) == ACC_INTERFACE;

        if (isAbstract || isInterface || !shouldTraceClass(name)) {
            mMethodsToCheck.clear();
            return;
        }
    }

    @Override
    public MethodVisitor visitMethod(
            int access, String name, String descriptor, String signature, String[] exceptions) {
        if (mMethodsToCheck.isEmpty()) {
            return super.visitMethod(access, name, descriptor, signature, exceptions);
        }

        for (MethodDescription method : mMethodsToCheck) {
            if (method.methodName.equals(name) && method.description.equals(descriptor)) {
                method.shouldCreateOverride = false;
            }
        }

        return super.visitMethod(access, name, descriptor, signature, exceptions);
    }

    @Override
    public void visitEnd() {
        if (mMethodsToCheck.isEmpty()) {
            super.visitEnd();
            return;
        }

        boolean areAnyUncheckedMethods = false;

        for (MethodDescription method : mMethodsToCheck) {
            if (method.shouldCreateOverride == null) {
                areAnyUncheckedMethods = true;
                break;
            }
        }

        if (areAnyUncheckedMethods) {
            checkParentClass(mSuperName, mMethodsToCheck, mClassHierarchy);
        }

        super.visitEnd();
    }

    private boolean shouldTraceClass(String desc) {
        return mClassHierarchy.isAssignableFrom(VIEW_CLASS_DESCRIPTOR, desc)
                || mClassHierarchy.isAssignableFrom(ANIMATOR_UPDATE_LISTENER_CLASS_DESCRIPTOR, desc)
                || mClassHierarchy.isAssignableFrom(ANIMATOR_LISTENER_CLASS_DESCRIPTOR, desc);
    }

    static void checkParentClass(
            String superClassName,
            ArrayList<MethodDescription> methodsToCheck,
            ClassHierarchy classHierarchy) {
        if (superClassName == null || "java/lang/Object".equals(superClassName)) {
            // We support tracing methods that are defined in classes that are derived from View,
            // but are not defined in View itself. If we've reached Object, the method doesn't
            // exist in this hierarchy, so don't override it.
            for (MethodDescription method : methodsToCheck) {
                if (method.shouldCreateOverride == null) {
                    method.shouldCreateOverride = false;
                }
            }
            return;
        }
        byte[] bytes = classHierarchy.getClassBytes(superClassName);
        if (bytes == null) {
            // Skip tracing classes with missing superclasses to avoid generating overrides or
            // failing frame computation on incomplete hierarchies.
            methodsToCheck.clear();
            return;
        }
        ClassReader cr = new ClassReader(bytes);
        ParentMethodCheckerClassAdapter parentChecker =
                new ParentMethodCheckerClassAdapter(methodsToCheck, classHierarchy);
        cr.accept(parentChecker, EXPAND_FRAMES);
    }
}
