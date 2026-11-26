// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.Opcodes;

import org.chromium.bytecode.MethodCheckerClassAdapter.ParentCache;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Java application that modifies all implementations of "draw", "onMeasure" and "onLayout" on all
 * {@link android.view.View} subclasses to wrap them in trace events.
 */
public class TraceEventAdder extends ByteCodeRewriter {
    // Cap worker threads because this runs as one build step alongside other compile/dex actions.
    private static final int MAX_WORKER_THREADS = 4;

    private final ParentCache mParentCache;
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
            ParentCache parentCache = new ParentCache(classHierarchy);
            rewriteAll(classHierarchy, parentCache, inputJars, outputJars);
        }
    }

    private static void rewriteAll(
            ClassHierarchy classHierarchy,
            ParentCache parentCache,
            String[] inputJars,
            String[] outputJars)
            throws IOException {
        int numThreads =
                Math.min(
                        outputJars.length,
                        Math.min(Runtime.getRuntime().availableProcessors(), MAX_WORKER_THREADS));
        AtomicInteger nextIndex = new AtomicInteger();
        AtomicReference<Throwable> firstError = new AtomicReference<>();
        Runnable workerTask =
                () -> {
                    TraceEventAdder adder = new TraceEventAdder(classHierarchy, parentCache);
                    while (firstError.get() == null) {
                        int idx = nextIndex.getAndIncrement();
                        if (idx >= outputJars.length) {
                            break;
                        }
                        try {
                            adder.rewrite(new File(inputJars[idx]), new File(outputJars[idx]));
                        } catch (Throwable e) {
                            firstError.compareAndSet(null, e);
                            return;
                        }
                    }
                };
        Thread[] workers = new Thread[Math.max(0, numThreads - 1)];
        for (int t = 0; t < workers.length; t++) {
            workers[t] = new Thread(workerTask);
            workers[t].start();
        }
        workerTask.run();
        boolean interrupted = false;
        for (Thread worker : workers) {
            while (true) {
                try {
                    worker.join();
                    break;
                } catch (InterruptedException e) {
                    interrupted = true;
                    firstError.compareAndSet(null, e);
                }
            }
        }
        if (interrupted) {
            Thread.currentThread().interrupt();
        }
        Throwable err = firstError.get();
        if (err instanceof IOException) {
            throw (IOException) err;
        }
        if (err instanceof RuntimeException) {
            throw (RuntimeException) err;
        }
        if (err instanceof Error) {
            throw (Error) err;
        }
        if (err != null) {
            throw new RuntimeException(err);
        }
    }

    TraceEventAdder(ClassHierarchy classHierarchy, ParentCache parentCache) {
        super(classHierarchy);
        mParentCache = parentCache;
    }

    @Override
    protected boolean shouldRewriteClass(String classPath) {
        return true;
    }

    @Override
    protected boolean shouldRewriteClass(ClassReader classReader) {
        int access = classReader.getAccess();
        if ((access & (Opcodes.ACC_ABSTRACT | Opcodes.ACC_INTERFACE)) != 0
                || !MethodCheckerClassAdapter.shouldTraceClass(
                        classReader.getClassName(), getClassHierarchy())) {
            return false;
        }

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
        // class and which ones need to be overridden.
        MethodCheckerClassAdapter methodChecker =
                new MethodCheckerClassAdapter(mMethodsToTrace, mParentCache);

        classReader.accept(
                methodChecker,
                ClassReader.SKIP_CODE | ClassReader.SKIP_DEBUG | ClassReader.SKIP_FRAMES);

        return !mMethodsToTrace.isEmpty();
    }

    @Override
    protected ClassVisitor getClassVisitorForClass(String classPath, ClassVisitor delegate) {
        ClassVisitor chain = new TraceEventAdderClassAdapter(delegate, mMethodsToTrace);
        chain = new EmptyOverrideGeneratorClassAdapter(chain, mMethodsToTrace);

        return chain;
    }
}
