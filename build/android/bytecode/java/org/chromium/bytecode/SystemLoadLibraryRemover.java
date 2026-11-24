// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;

/** Java application that removes all calls to System.loadLibrary() from all classes in a jar. */
public class SystemLoadLibraryRemover extends ByteCodeRewriter {

    public static void main(String[] args) throws IOException {
        ClassHierarchy.ParsedArgs parsed = ClassHierarchy.ParsedArgs.parse(args);
        String[] positionalArgs = parsed.positionalArgs;

        if (positionalArgs.length < 2) {
            System.err.println(
                    "Expected arguments: [--classpath <cp>]"
                            + " <':' separated list of input jar paths>"
                            + " <':' separated list of output jar paths>");
            System.exit(1);
        }

        String[] inputJars = positionalArgs[0].split(":");
        String[] outputJars = positionalArgs[1].split(":");

        if (inputJars.length < outputJars.length) {
            throw new IllegalArgumentException(
                    "Input list must be a superset of the output list, where the "
                            + "first N entries match, and N is the length of the output list. "
                            + "Inputs: "
                            + inputJars.length
                            + " Outputs: "
                            + outputJars.length);
        }

        ArrayList<String> classPathJarsPaths = new ArrayList<>(parsed.classpath);
        classPathJarsPaths.addAll(Arrays.asList(inputJars));

        String initializerClass = positionalArgs.length > 2 ? positionalArgs[2] : null;
        try (ClassHierarchy classHierarchy = new ClassHierarchy(classPathJarsPaths)) {
            SystemLoadLibraryRemover remover =
                    new SystemLoadLibraryRemover(classHierarchy, initializerClass);
            for (int i = 0; i < outputJars.length; i++) {
                remover.rewrite(new File(inputJars[i]), new File(outputJars[i]));
            }
        }
    }

    private final String mInitializerClass;

    SystemLoadLibraryRemover(ClassHierarchy classHierarchy, String initializerClass) {
        super(classHierarchy);
        mInitializerClass = initializerClass != null ? initializerClass.replace('.', '/') : null;
    }

    @Override
    protected boolean shouldRewriteClass(String classPath) {
        return true;
    }

    @Override
    protected ClassVisitor getClassVisitorForClass(String classPath, ClassVisitor delegate) {
        return new ClassVisitor(Opcodes.ASM9, delegate) {
            @Override
            public MethodVisitor visitMethod(
                    int access,
                    String name,
                    String descriptor,
                    String signature,
                    String[] exceptions) {
                MethodVisitor mv =
                        super.visitMethod(access, name, descriptor, signature, exceptions);
                return new MethodVisitor(Opcodes.ASM9, mv) {
                    @Override
                    public void visitMethodInsn(
                            int opcode,
                            String owner,
                            String name,
                            String descriptor,
                            boolean isInterface) {
                        if (opcode == Opcodes.INVOKESTATIC
                                && "java/lang/System".equals(owner)
                                && "loadLibrary".equals(name)
                                && "(Ljava/lang/String;)V".equals(descriptor)) {
                            // Pop the library name string argument.
                            super.visitInsn(Opcodes.POP);
                            if (mInitializerClass != null) {
                                super.visitMethodInsn(
                                        Opcodes.INVOKESTATIC,
                                        mInitializerClass,
                                        "ensureInitialized",
                                        "()V",
                                        false);
                            }
                        } else {
                            super.visitMethodInsn(opcode, owner, name, descriptor, isInterface);
                        }
                    }
                };
            }
        };
    }
}
