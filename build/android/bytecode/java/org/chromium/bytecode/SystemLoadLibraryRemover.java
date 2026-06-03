// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;

import java.io.File;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;

/** Java application that removes all calls to System.loadLibrary() from all classes in a jar. */
public class SystemLoadLibraryRemover extends ByteCodeRewriter {

    static ClassLoader loadJars(Collection<String> paths) {
        URL[] jarUrls = new URL[paths.size()];
        int i = 0;
        for (String path : paths) {
            try {
                jarUrls[i++] = new File(path).toURI().toURL();
            } catch (MalformedURLException e) {
                throw new RuntimeException(e);
            }
        }
        return new URLClassLoader(jarUrls);
    }

    public static void main(String[] args) throws IOException {
        args = ByteCodeRewriter.expandArgs(args);

        if (args.length < 2) {
            System.err.println(
                    "Expected arguments: <':' separated list of input jar paths> "
                            + "<':' separated list of output jar paths>");
            System.exit(1);
        }

        String[] inputJars = args[0].split(":");
        String[] outputJars = args[1].split(":");

        assert inputJars.length >= outputJars.length
                : "Input list must be a superset of the output list, where the "
                        + "first N entries match, and N is the length of the output list.";

        ArrayList<String> classPathJarsPaths = new ArrayList<>();
        classPathJarsPaths.addAll(Arrays.asList(inputJars));
        ClassLoader classPathJarsClassLoader = loadJars(classPathJarsPaths);

        String initializerClass = args.length > 2 ? args[2] : null;
        SystemLoadLibraryRemover remover =
                new SystemLoadLibraryRemover(classPathJarsClassLoader, initializerClass);
        for (int i = 0; i < outputJars.length; i++) {
            remover.rewrite(new File(inputJars[i]), new File(outputJars[i]));
        }
    }

    private final String mInitializerClass;

    SystemLoadLibraryRemover(ClassLoader classLoader, String initializerClass) {
        super(classLoader);
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
