// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.Opcodes;

import java.io.File;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;

/**
 * Java application that modifies all implementations of "draw", "onMeasure" and "onLayout" on all
 * {@link android.view.View} subclasses to wrap them in trace events.
 */
public class TraceEventAdder extends ByteCodeRewriter {
    private final ClassLoader mClassPathJarsClassLoader;
    private ArrayList<MethodDescription> mMethodsToTrace;

    /**
     * Loads a list of jars and returns a ClassLoader capable of loading all classes found in the
     * given jars.
     */
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
        // Invoke this script using //build/android/gyp/trace_event_bytecode_rewriter.py
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
                        + "first N entries match, and N is the length of the output list."
                        + inputJars.length
                        + " Outputs: "
                        + outputJars.length;

        // outputJars[n] must be the same as inputJars[n] but with a suffix, validate this.
        for (int i = 0; i < outputJars.length; i++) {
            File inputJarPath = new File(inputJars[i]);
            String inputJarFilename = inputJarPath.getName();
            File outputJarPath = new File(outputJars[i]);

            String inputFilenameNoExtension =
                    inputJarFilename.substring(0, inputJarFilename.lastIndexOf(".jar"));

            // Ensuring that for every output, we have the matching input.
            assert outputJarPath.getName().startsWith(inputFilenameNoExtension);
        }

        ArrayList<String> classPathJarsPaths = new ArrayList<>();
        classPathJarsPaths.addAll(Arrays.asList(inputJars));
        ClassLoader classPathJarsClassLoader = loadJars(classPathJarsPaths);

        TraceEventAdder adder = new TraceEventAdder(classPathJarsClassLoader);
        for (int i = 0; i < outputJars.length; i++) {
            adder.rewrite(new File(inputJars[i]), new File(outputJars[i]));
        }
    }

    public TraceEventAdder(ClassLoader classPathJarsClassLoader) {
        mClassPathJarsClassLoader = classPathJarsClassLoader;
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
