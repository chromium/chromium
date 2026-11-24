// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import static org.objectweb.asm.Opcodes.ACC_FINAL;
import static org.objectweb.asm.Opcodes.ACC_PRIVATE;
import static org.objectweb.asm.Opcodes.ACC_PROTECTED;
import static org.objectweb.asm.Opcodes.ACC_PUBLIC;
import static org.objectweb.asm.Opcodes.ASM9;

import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.MethodVisitor;

import java.util.ArrayList;

/**
 * This ClassVisitor checks if the given class overrides methods on {@code methodsToCheck}, and if
 * so it determines whether they can be overridden by a child class. If at the end any unchecked
 * methods remain then we recurse on the class's superclass.
 */
class ParentMethodCheckerClassAdapter extends ClassVisitor {
    private final ArrayList<MethodDescription> mMethodsToCheck;
    private final ClassHierarchy mClassHierarchy;
    private String mSuperName;

    public ParentMethodCheckerClassAdapter(
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
    }

    @Override
    public MethodVisitor visitMethod(
            int access, String name, String descriptor, String signature, String[] exceptions) {
        for (MethodDescription methodToCheck : mMethodsToCheck) {
            if (methodToCheck.shouldCreateOverride != null
                    || !methodToCheck.methodName.equals(name)
                    || !methodToCheck.description.equals(descriptor)) {
                continue;
            }

            // This class contains methodToCheck.
            boolean isMethodPrivate = (access & ACC_PRIVATE) == ACC_PRIVATE;
            boolean isMethodFinal = (access & ACC_FINAL) == ACC_FINAL;
            boolean isMethodPackagePrivate =
                    (access & (ACC_PUBLIC | ACC_PROTECTED | ACC_PRIVATE)) == 0;

            // If the method is private or final then don't create an override.
            methodToCheck.shouldCreateOverride =
                    !isMethodPrivate && !isMethodFinal && !isMethodPackagePrivate;
        }

        return super.visitMethod(access, name, descriptor, signature, exceptions);
    }

    @Override
    public void visitEnd() {
        boolean areAnyUncheckedMethods = false;

        for (MethodDescription method : mMethodsToCheck) {
            if (method.shouldCreateOverride == null) {
                areAnyUncheckedMethods = true;
                break;
            }
        }

        if (areAnyUncheckedMethods) {
            MethodCheckerClassAdapter.checkParentClass(
                    mSuperName, mMethodsToCheck, mClassHierarchy);
        }

        super.visitEnd();
    }
}
