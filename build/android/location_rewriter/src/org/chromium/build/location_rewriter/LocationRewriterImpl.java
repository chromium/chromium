// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.location_rewriter;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.commons.AdviceAdapter;

public class LocationRewriterImpl {
    /**
     * Rewrite the contents of the input class by replacing invocations of methods listed in {@link
     * org.chromium.build.location_rewriter.LocationRewriterTargetMethods} with the corresponding
     * overload that takes in an additional Location object and also adds code to generate the
     * Location object populated with the source file name, function name and line number of the
     * callsite.
     *
     * @param input the contents of the input file
     * @return output the rewritten input data. This is equal to the {@param input} object if no
     *     rewriting was done.
     */
    public byte[] rewrite(byte[] input) {
        ClassReader classReader = new ClassReader(input);
        return needsRewriting(classReader) ? rewriteClass(classReader) : input;
    }

    private static boolean needsRewriting(ClassReader classReader) {
        ScanningClassVisitor scanner = new ScanningClassVisitor();
        classReader.accept(scanner, ClassReader.SKIP_DEBUG | ClassReader.SKIP_FRAMES);
        return scanner.mNeedsRewriting;
    }

    private byte[] rewriteClass(ClassReader classReader) {
        ClassWriter classWriter = new ClassWriter(classReader, ClassWriter.COMPUTE_FRAMES);
        ClassVisitor transformer = new RewritingClassVisitor(classWriter);
        classReader.accept(transformer, ClassReader.EXPAND_FRAMES);
        return classWriter.toByteArray();
    }

    /** Class visitor that scans for method invocations that require rewriting. */
    private static class ScanningClassVisitor extends ClassVisitor {
        private boolean mNeedsRewriting = false;

        public ScanningClassVisitor() {
            super(Opcodes.ASM9);
        }

        @Override
        public MethodVisitor visitMethod(
                int access, String name, String descriptor, String signature, String[] exceptions) {
            if (mNeedsRewriting) {
                // Don't bother checking other methods if transformation is known to be needed
                return null;
            }

            return new ScanningMethodVisitor(this);
        }
    }

    /** Method visitor that scans for method invocations that require rewriting. */
    private static class ScanningMethodVisitor extends MethodVisitor {
        private final ScanningClassVisitor mClassVisitor;

        ScanningMethodVisitor(ScanningClassVisitor classVisitor) {
            super(Opcodes.ASM9);
            this.mClassVisitor = classVisitor;
        }

        @Override
        public void visitMethodInsn(
                int opcode, String owner, String name, String descriptor, boolean isInterface) {
            VisitableMethod current =
                    new VisitableMethod(opcode, owner, name, descriptor, isInterface);
            if (LocationRewriterTargetMethods.getOverload(current) != null) {
                this.mClassVisitor.mNeedsRewriting = true;
            }
        }
    }

    /** Class visitor that rewrites method invocations. */
    private static class RewritingClassVisitor extends ClassVisitor {
        private String mSourceFileName;
        private String mPackagePath;

        private RewritingClassVisitor(ClassVisitor classVisitor) {
            super(Opcodes.ASM9, classVisitor);
        }

        @Override
        public void visit(
                int version,
                int access,
                String name,
                String signature,
                String superName,
                String[] interfaces) {
            int lastSlash = name.lastIndexOf('/');
            if (lastSlash != -1) {
                // Prepend the package path to the simple file name.
                this.mPackagePath = name.substring(0, lastSlash);
            }
            super.visit(version, access, name, signature, superName, interfaces);
        }

        @Override
        public void visitSource(String source, String debug) {
            this.mSourceFileName = source;
            super.visitSource(source, debug);
        }

        @Override
        public MethodVisitor visitMethod(
                int access, String name, String descriptor, String signature, String[] exceptions) {
            MethodVisitor methodVisitor =
                    super.visitMethod(access, name, descriptor, signature, exceptions);
            if (methodVisitor == null) {
                return null;
            }
            String sourceFileName;
            if (mSourceFileName != null) {
                if (mPackagePath != null) {
                    sourceFileName = mPackagePath + "/" + mSourceFileName;
                } else {
                    sourceFileName = mSourceFileName;
                }
            } else {
                sourceFileName = "(unknown).java";
            }
            return new RewritingMethodVisitor(
                    methodVisitor, access, name, descriptor, sourceFileName);
        }
    }

    /** Method visitor that rewrites method invocations. */
    private static class RewritingMethodVisitor extends AdviceAdapter {
        private final String mMethodName;
        private final String mSourceFileName;
        private int mCurrentLineNumber;

        RewritingMethodVisitor(
                MethodVisitor methodVisitor,
                int access,
                String name,
                String descriptor,
                String sourceFileName) {
            super(Opcodes.ASM9, methodVisitor, access, name, descriptor);
            this.mMethodName = name;
            this.mSourceFileName = sourceFileName;
            this.mCurrentLineNumber = 0;
        }

        @Override
        public void visitLineNumber(int line, org.objectweb.asm.Label start) {
            this.mCurrentLineNumber = line;
            super.visitLineNumber(line, start);
        }

        @Override
        public void visitMethodInsn(
                int opcode, String owner, String name, String descriptor, boolean isInterface) {
            VisitableMethod overload =
                    LocationRewriterTargetMethods.getOverload(
                            new VisitableMethod(opcode, owner, name, descriptor, isInterface));
            if (overload == null) {
                // This is not a method we need to rewrite, pass it on.
                super.visitMethodInsn(opcode, owner, name, descriptor, isInterface);
                return;
            }

            // The arguments for the original method are already on the stack. Only the location
            // object needs to be pushed on the stack before calling the overload. This is done by
            // visiting the Location.from() method, which requires its arguments to be pushed on the
            // stack before being visited and will push the returned Location object on the stack
            // after being visited.
            super.visitLdcInsn(mSourceFileName);
            super.visitLdcInsn(mMethodName);
            super.visitLdcInsn(mCurrentLineNumber);
            LocationClass.FROM_METHOD.visit(this);
            overload.visit(this);
        }
    }
}
