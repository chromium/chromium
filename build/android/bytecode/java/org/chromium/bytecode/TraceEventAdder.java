// Copyright 2021 The Chromium Authors
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
    private ArrayList<MethodDescription> mMethodsToTrace;

    public static void main(String[] args) throws IOException {
        // Invoke this script using //build/android/gyp/trace_event_bytecode_rewriter.py
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

        // outputJars[n] must be the same as inputJars[n] but with a suffix, validate this.
        for (int i = 0; i < outputJars.length; i++) {
            String inputJarFilename = new File(inputJars[i]).getName();
            String outputJarFilename = new File(outputJars[i]).getName();
            int jarExtIdx = inputJarFilename.lastIndexOf(".jar");
            if (jarExtIdx < 0) {
                throw new IllegalArgumentException(
                        "Expected .jar input path, got: " + inputJars[i]);
            }
            String inputFilenameNoExtension = inputJarFilename.substring(0, jarExtIdx);

            // Ensuring that for every output, we have the matching input.
            if (!outputJarFilename.startsWith(inputFilenameNoExtension)) {
                throw new IllegalArgumentException(
                        "Output jar "
                                + outputJarFilename
                                + " does not start with "
                                + inputFilenameNoExtension);
            }
        }

        ArrayList<String> classPathJarsPaths = new ArrayList<>(parsed.classpath);
        classPathJarsPaths.addAll(Arrays.asList(inputJars));

        try (ClassHierarchy classHierarchy = new ClassHierarchy(classPathJarsPaths)) {
            TraceEventAdder adder = new TraceEventAdder(classHierarchy);
            for (int i = 0; i < outputJars.length; i++) {
                adder.rewrite(new File(inputJars[i]), new File(outputJars[i]));
            }
        }
    }

    TraceEventAdder(ClassHierarchy classHierarchy) {
        super(classHierarchy);
    }

    @Override
    protected boolean shouldRewriteClass(String classPath) {
        return true;
    }

    @Override
    protected boolean shouldRewriteClass(ClassReader classReader) {
        mMethodsToTrace =
                new ArrayList<>(
                        Arrays.asList(
                                // Methods on View.java
                                new MethodDescription(
                                        "dispatchTouchEvent",
                                        "(Landroid/view/MotionEvent;)Z",
                                        Opcodes.ACC_PUBLIC),
                                new MethodDescription(
                                        "draw", "(Landroid/graphics/Canvas;)V", Opcodes.ACC_PUBLIC),
                                new MethodDescription("onMeasure", "(II)V", Opcodes.ACC_PROTECTED),
                                new MethodDescription(
                                        "onLayout", "(ZIIII)V", Opcodes.ACC_PROTECTED),
                                // Methods on RecyclerView.java in AndroidX
                                new MethodDescription("scrollStep", "(II[I)V", 0),
                                // Methods on Animator.AnimatorListener
                                new MethodDescription(
                                        "onAnimationStart",
                                        "(Landroid/animation/Animator;)V",
                                        Opcodes.ACC_PUBLIC),
                                new MethodDescription(
                                        "onAnimationEnd",
                                        "(Landroid/animation/Animator;)V",
                                        Opcodes.ACC_PUBLIC),
                                // Methods on ValueAnimator.AnimatorUpdateListener
                                new MethodDescription(
                                        "onAnimationUpdate",
                                        "(Landroid/animation/ValueAnimator;)V",
                                        Opcodes.ACC_PUBLIC)));

        // This adapter will modify mMethodsToTrace to indicate which methods already exist in the
        // class and which ones need to be overridden. In case the class is not an Android view
        // we'll clear the list and skip rewriting.
        MethodCheckerClassAdapter methodChecker =
                new MethodCheckerClassAdapter(mMethodsToTrace, getClassHierarchy());

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
