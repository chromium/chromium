// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.Opcodes;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;

/**
 * Java application that modifies all implementations of "draw", "onMeasure" and "onLayout" on all
 * {@link android.view.View} subclasses to wrap them in trace events.
 */
public class TraceEventAdder extends ByteCodeRewriter {
    private final ClassLoader mClassPathJarsClassLoader;
    private ArrayList<MethodDescription> mMethodsToTrace;

    public static void main(String[] args) throws IOException {
        // Invoke this script using //build/android/gyp/bytecode_rewriter.py
        if (args.length < 2) {
            System.err.println(
                    "Expected arguments: <input.jar> <output.jar> <input classpath jars>");
            System.exit(1);
        }

        String input = args[0];
        String output = args[1];

        ArrayList<String> classPathJarsPaths = new ArrayList<>();
        classPathJarsPaths.add(input);
        classPathJarsPaths.addAll(Arrays.asList(Arrays.copyOfRange(args, 2, args.length)));
        ClassLoader classPathJarsClassLoader = ByteCodeProcessor.loadJars(classPathJarsPaths);

        TraceEventAdder adder = new TraceEventAdder(classPathJarsClassLoader);
        adder.rewrite(new File(input), new File(output));
    }

    public TraceEventAdder(ClassLoader classPathJarsClassLoader) {
        mClassPathJarsClassLoader = classPathJarsClassLoader;
    }

    @Override
    protected boolean shouldRewriteClass(String classPath) {
        try {
            // If this jar's dependencies can't find Chromium's TraceEvent class then skip this
            // class. Conceptually this could be fixed by adding a dependency on //base:base_java
            // but that would cause circular dependencies and any changes to base_java would cause
            // all android_library targets to require rebuilding.
            mClassPathJarsClassLoader.loadClass("org.chromium.base.TraceEvent");
            return true;
        } catch (ClassNotFoundException ex) {
            return false;
        }
    }

    @Override
    protected boolean shouldRewriteClass(ClassReader classReader) {
        mMethodsToTrace = new ArrayList<>(Arrays.asList(
                new MethodDescription("draw", "(Landroid/graphics/Canvas;)V", Opcodes.ACC_PUBLIC),
                new MethodDescription("onMeasure", "(II)V", Opcodes.ACC_PROTECTED),
                new MethodDescription("onLayout", "(ZIIII)V", Opcodes.ACC_PROTECTED)));

        // This adapter will modify mMethodsToTrace to indicate which methods already exist in the
        // class and which ones need to be overridden. In case the class is not an Android view
        // we'll clear the list and skip rewriting.
        MethodCheckerClassAdapter methodChecker =
                new MethodCheckerClassAdapter(mMethodsToTrace, mClassPathJarsClassLoader);

        classReader.accept(methodChecker, ClassReader.EXPAND_FRAMES);

        return !mMethodsToTrace.isEmpty();
    }

    @Override
    protected ClassVisitor getClassVisitorForClass(String classPath, ClassVisitor delegate) {
        ClassVisitor chain = new TraceEventAdderClassAdapter(delegate, mMethodsToTrace);
        chain = new EmptyOverrideGeneratorClassAdapter(chain, mMethodsToTrace);

        return chain;
    }
}
