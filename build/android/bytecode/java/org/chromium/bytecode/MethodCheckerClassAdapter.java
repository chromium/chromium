// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import static org.objectweb.asm.ClassReader.EXPAND_FRAMES;
import static org.objectweb.asm.Opcodes.ACC_ABSTRACT;
import static org.objectweb.asm.Opcodes.ACC_INTERFACE;
import static org.objectweb.asm.Opcodes.ASM7;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.MethodVisitor;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;

/**
 * This ClassVisitor verifies that a class and its methods are suitable for rewriting.
 * Given a class and a list of methods it performs the following checks:
 * 1. Class is subclass of {@link android.view.View}.
 * 2. Class is not abstract or an interface.
 *
 * For each method provided in {@code methodsToCheck}:
 * If the class overrides the method then we can rewrite it directly.
 * If the class doesn't override the method then we can generate an override with {@link
 * EmptyOverrideGeneratorClassAdapter}, but first we must check if the parent method is private or
 * final using {@link ParentMethodCheckerClassAdapter}.
 *
 * This adapter modifies the provided method list to indicate which methods should be overridden or
 * skipped.
 */
class MethodCheckerClassAdapter extends ClassVisitor {
    private static final String VIEW_CLASS_DESCRIPTOR = "android/view/View";

    private final ArrayList<MethodDescription> mMethodsToCheck;
    private final ClassLoader mJarClassLoader;
    private String mSuperName;

    public MethodCheckerClassAdapter(
            ArrayList<MethodDescription> methodsToCheck, ClassLoader jarClassLoader) {
        super(ASM7);
        mMethodsToCheck = methodsToCheck;
        mJarClassLoader = jarClassLoader;
    }

    @Override
    public void visit(int version, int access, String name, String signature, String superName,
            String[] interfaces) {
        super.visit(version, access, name, signature, superName, interfaces);

        mSuperName = superName;

        boolean isAbstract = (access & ACC_ABSTRACT) == ACC_ABSTRACT;
        boolean isInterface = (access & ACC_INTERFACE) == ACC_INTERFACE;

        if (isAbstract || isInterface || !isClassView(name)) {
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
            checkParentClass(mSuperName, mMethodsToCheck, mJarClassLoader);
        }

        super.visitEnd();
    }

    private boolean isClassView(String desc) {
        Class currentClass = getClass(desc);
        Class viewClass = getClass(VIEW_CLASS_DESCRIPTOR);
        if (currentClass != null && viewClass != null) {
            return viewClass.isAssignableFrom(currentClass);
        }
        return false;
    }

    private Class getClass(String desc) {
        try {
            return mJarClassLoader.loadClass(desc.replace('/', '.'));
        } catch (ClassNotFoundException | NoClassDefFoundError | IllegalAccessError e) {
            return null;
        }
    }

    static void checkParentClass(String superClassName, ArrayList<MethodDescription> methodsToCheck,
            ClassLoader jarClassLoader) {
        try {
            ClassReader cr = new ClassReader(getClassAsStream(jarClassLoader, superClassName));
            ParentMethodCheckerClassAdapter parentChecker =
                    new ParentMethodCheckerClassAdapter(methodsToCheck, jarClassLoader);
            cr.accept(parentChecker, EXPAND_FRAMES);
        } catch (IOException ex) {
            // Ignore errors in case class can't be loaded.
        }
    }

    private static InputStream getClassAsStream(ClassLoader jarClassLoader, String desc) {
        return jarClassLoader.getResourceAsStream(desc.replace('.', '/') + ".class");
    }
}
