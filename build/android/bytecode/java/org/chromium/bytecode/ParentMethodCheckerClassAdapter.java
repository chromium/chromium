// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import static org.objectweb.asm.Opcodes.ACC_FINAL;
import static org.objectweb.asm.Opcodes.ACC_PRIVATE;
import static org.objectweb.asm.Opcodes.ASM7;

import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.MethodVisitor;

import java.util.ArrayList;

/**
 * This ClassVisitor checks if the given class overrides methods on {@code methodsToCheck}, and if
 * so it determines whether they can be overridden by a child class. If at the end any unchecked
 * methods remain then we recurse on the class's superclass.
 */
class ParentMethodCheckerClassAdapter extends ClassVisitor {
    private static final String OBJECT_CLASS_DESCRIPTOR = "java.lang.Object";

    private final ArrayList<MethodDescription> mMethodsToCheck;
    private final ClassLoader mJarClassLoader;
    private String mSuperName;
    private boolean mIsCheckingObjectClass;

    public ParentMethodCheckerClassAdapter(
            ArrayList<MethodDescription> methodsToCheck, ClassLoader jarClassLoader) {
        super(ASM7);
        mMethodsToCheck = methodsToCheck;
        mJarClassLoader = jarClassLoader;
    }

    @Override
    public void visit(int version, int access, String name, String signature, String superName,
            String[] interfaces) {
        super.visit(version, access, name, signature, superName, interfaces);

        if (name.equals(OBJECT_CLASS_DESCRIPTOR)) {
            mIsCheckingObjectClass = true;
            return;
        }

        mSuperName = superName;
    }

    @Override
    public MethodVisitor visitMethod(
            int access, String name, String descriptor, String signature, String[] exceptions) {
        if (mIsCheckingObjectClass) {
            return super.visitMethod(access, name, descriptor, signature, exceptions);
        }

        for (MethodDescription methodToCheck : mMethodsToCheck) {
            if (methodToCheck.shouldCreateOverride != null || !methodToCheck.methodName.equals(name)
                    || !methodToCheck.description.equals(descriptor)) {
                continue;
            }

            // This class contains methodToCheck.
            boolean isMethodPrivate = (access & ACC_PRIVATE) == ACC_PRIVATE;
            boolean isMethodFinal = (access & ACC_FINAL) == ACC_FINAL;
            // If the method is private or final then don't create an override.
            methodToCheck.shouldCreateOverride = !isMethodPrivate && !isMethodFinal;
        }

        return super.visitMethod(access, name, descriptor, signature, exceptions);
    }

    @Override
    public void visitEnd() {
        if (mIsCheckingObjectClass) {
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
            MethodCheckerClassAdapter.checkParentClass(
                    mSuperName, mMethodsToCheck, mJarClassLoader);
        }

        super.visitEnd();
    }
}
