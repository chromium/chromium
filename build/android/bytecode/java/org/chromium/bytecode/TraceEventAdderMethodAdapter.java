// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import static org.objectweb.asm.Opcodes.ASM7;
import static org.objectweb.asm.Opcodes.ATHROW;
import static org.objectweb.asm.Opcodes.INVOKESTATIC;
import static org.objectweb.asm.Opcodes.IRETURN;
import static org.objectweb.asm.Opcodes.RETURN;

import static org.chromium.bytecode.TypeUtils.STRING;
import static org.chromium.bytecode.TypeUtils.VOID;

import org.objectweb.asm.MethodVisitor;

/**
 * MethodVisitor that wraps all code in TraceEvent.begin and TraceEvent.end calls. TraceEvent.end
 * calls are added on all returns and thrown exceptions.
 *
 * Example:
 * <pre>
 *   {@code
 *      int methodToTrace(String foo){
 *
 *        //Line added by rewriter:
 *        TraceEvent.begin("ClassName.methodToTrace");
 *
 *        if(foo == null){
 *          //Line added by rewriter:
 *          TraceEvent.end("ClassName.methodToTrace");
 *
 *          throw new Exception();
 *        }
 *        else if(foo.equals("Two")){
 *          //Line added by rewriter:
 *          TraceEvent.end("ClassName.methodToTrace");
 *
 *          return 2;
 *        }
 *
 *        //Line added by rewriter:
 *        TraceEvent.end("ClassName.methodToTrace");
 *
 *        return 0;
 *      }
 *   }
 * </pre>
 *
 */
class TraceEventAdderMethodAdapter extends MethodVisitor {
    private static final String TRACE_EVENT_DESCRIPTOR = "org/chromium/base/TraceEvent";
    private static final String TRACE_EVENT_SIGNATURE = TypeUtils.getMethodDescriptor(VOID, STRING);
    private final String mEventName;

    public TraceEventAdderMethodAdapter(
            MethodVisitor methodVisitor, String shortClassName, String methodName) {
        super(ASM7, methodVisitor);

        mEventName = shortClassName + "." + methodName;
    }

    @Override
    public void visitCode() {
        super.visitCode();

        mv.visitLdcInsn(mEventName);
        mv.visitMethodInsn(
                INVOKESTATIC, TRACE_EVENT_DESCRIPTOR, "begin", TRACE_EVENT_SIGNATURE, false);
    }

    @Override
    public void visitInsn(int opcode) {
        if ((opcode >= IRETURN && opcode <= RETURN) || opcode == ATHROW) {
            mv.visitLdcInsn(mEventName);
            mv.visitMethodInsn(
                    INVOKESTATIC, TRACE_EVENT_DESCRIPTOR, "end", TRACE_EVENT_SIGNATURE, false);
        }

        mv.visitInsn(opcode);
    }
}
