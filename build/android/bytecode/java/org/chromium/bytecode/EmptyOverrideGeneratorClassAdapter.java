// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import static org.objectweb.asm.Opcodes.ACC_ABSTRACT;
import static org.objectweb.asm.Opcodes.ACC_INTERFACE;
import static org.objectweb.asm.Opcodes.ALOAD;
import static org.objectweb.asm.Opcodes.ASM7;
import static org.objectweb.asm.Opcodes.ILOAD;
import static org.objectweb.asm.Opcodes.INVOKESPECIAL;
import static org.objectweb.asm.Opcodes.IRETURN;

import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Type;

import java.util.ArrayList;

class EmptyOverrideGeneratorClassAdapter extends ClassVisitor {
    private final ArrayList<MethodDescription> mMethodsToGenerate;
    private String mSuperClassName;
    private boolean mIsAbstract;
    private boolean mIsInterface;

    public EmptyOverrideGeneratorClassAdapter(
            ClassVisitor cv, ArrayList<MethodDescription> methodsToGenerate) {
        super(ASM7, cv);
        mMethodsToGenerate = methodsToGenerate;
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

        mSuperClassName = superName;
        mIsAbstract = (access & ACC_ABSTRACT) == ACC_ABSTRACT;
        mIsInterface = (access & ACC_INTERFACE) == ACC_INTERFACE;
    }

    @Override
    public void visitEnd() {
        if (mIsAbstract || mIsInterface || mMethodsToGenerate.isEmpty()) {
            super.visitEnd();
            return;
        }

        for (MethodDescription method : mMethodsToGenerate) {
            if (!method.shouldCreateOverride) {
                continue;
            }

            MethodVisitor mv =
                    super.visitMethod(
                            method.access, method.methodName, method.description, null, null);
            writeOverrideCode(mv, method.access, method.methodName, method.description);
        }

        super.visitEnd();
    }

    /**
     * Writes code to a method to call that method's parent implementation.
     * <pre>
     * {@code
     * // Calling writeOverrideCode(mv, ACC_PUBLIC, "doFoo", "(Ljava/lang/String;)I") writes the
     * following method body: public int doFoo(String arg){ return super.doFoo(arg);
     * }
     * }
     * </pre>
     *
     * This will be rewritten later by TraceEventAdderClassAdapter to wrap the body in a trace
     * event.
     */
    private void writeOverrideCode(
            MethodVisitor mv, final int access, final String name, final String descriptor) {
        assert access != 0;
        Type[] argTypes = Type.getArgumentTypes(descriptor);
        Type returnType = Type.getReturnType(descriptor);

        mv.visitCode();

        // Variable 0 contains `this`, load it into the operand stack.
        mv.visitVarInsn(ALOAD, 0);

        // Variables 1..n contain all arguments, load them all into the operand stack.
        int i = 1;
        for (Type arg : argTypes) {
            // getOpcode(ILOAD) returns the ILOAD equivalent to the current argument's type.
            mv.visitVarInsn(arg.getOpcode(ILOAD), i);
            i += arg.getSize();
        }

        // Call the parent class method with the same arguments.
        mv.visitMethodInsn(INVOKESPECIAL, mSuperClassName, name, descriptor, false);

        // Return the result.
        mv.visitInsn(returnType.getOpcode(IRETURN));

        mv.visitMaxs(0, 0);
        mv.visitEnd();
    }
}
