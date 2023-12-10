// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import static org.objectweb.asm.Opcodes.ASM7;

import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.MethodVisitor;

import java.util.ArrayList;

/**
 * A ClassVisitor for adding TraceEvent.begin and TraceEvent.end methods to any methods specified in
 * a list.
 */
class TraceEventAdderClassAdapter extends ClassVisitor {
    private final ArrayList<MethodDescription> mMethodsToTrace;
    private String mShortClassName;

    TraceEventAdderClassAdapter(ClassVisitor visitor, ArrayList<MethodDescription> methodsToTrace) {
        super(ASM7, visitor);
        mMethodsToTrace = methodsToTrace;
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
        mShortClassName = name.substring(name.lastIndexOf('/') + 1);
    }

    @Override
    public MethodVisitor visitMethod(
            final int access,
            final String name,
            String desc,
            String signature,
            String[] exceptions) {
        MethodVisitor mv = super.visitMethod(access, name, desc, signature, exceptions);

        for (MethodDescription method : mMethodsToTrace) {
            if (method.methodName.equals(name) && method.description.equals(desc)) {
                return new TraceEventAdderMethodAdapter(mv, mShortClassName, name);
            }
        }

        return mv;
    }
}
