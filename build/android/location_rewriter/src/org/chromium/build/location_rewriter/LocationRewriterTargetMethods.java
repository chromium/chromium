// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.location_rewriter;

import org.objectweb.asm.Opcodes;

import java.util.HashMap;
import java.util.Map;

/**
 * Class containing the mapping of methods to their corresponding overloads that take an additional
 * location object as the final argument.
 */
public final class LocationRewriterTargetMethods {
    private static final Map<VisitableMethod, VisitableMethod> OVERLOAD_MAP = new HashMap<>();

    static {
        // org.chromium.base.task.TaskRunner#postDelayedTask(Runnable, long)
        addRewriterTarget(
                Opcodes.INVOKEINTERFACE,
                "org/chromium/base/task/TaskRunner",
                "postDelayedTask",
                "(Ljava/lang/Runnable;J)V",
                /* isInterface= */ true);
        // org.chromium.base.task.TaskRunnerImpl#postDelayedTask(Runnable, long)
        addRewriterTarget(
                Opcodes.INVOKEVIRTUAL,
                "org/chromium/base/task/TaskRunnerImpl",
                "postDelayedTask",
                "(Ljava/lang/Runnable;J)V",
                /* isInterface= */ false);
        // org.chromium.base.task.PostTask#post(int, Runnable)
        addRewriterTarget(
                Opcodes.INVOKESTATIC,
                "org/chromium/base/task/PostTask",
                "postTask",
                "(ILjava/lang/Runnable;)V",
                /* isInterface= */ false);
        // org.chromium.base.task.PostTask#postDelayedTask(int, Runnable, long)
        addRewriterTarget(
                Opcodes.INVOKESTATIC,
                "org/chromium/base/task/PostTask",
                "postDelayedTask",
                "(ILjava/lang/Runnable;J)V",
                /* isInterface= */ false);
        // org.chromium.base.task.PostTask#runOrPostTask(int, Runnable)
        addRewriterTarget(
                Opcodes.INVOKESTATIC,
                "org/chromium/base/task/PostTask",
                "runOrPostTask",
                "(ILjava/lang/Runnable;)V",
                /* isInterface= */ false);
        // org.chromium.base.task.PostTask#runSynchronously(int, Callable<T>)
        addRewriterTarget(
                Opcodes.INVOKESTATIC,
                "org/chromium/base/task/PostTask",
                "runSynchronously",
                "(ILjava/util/concurrent/Callable;)Ljava/lang/Object;",
                /* isInterface= */ false);
        // org.chromium.base.task.PostTask#runSynchronously(int, Runnable)
        addRewriterTarget(
                Opcodes.INVOKESTATIC,
                "org/chromium/base/task/PostTask",
                "runSynchronously",
                "(ILjava/lang/Runnable;)V",
                /* isInterface= */ false);
        // org.chromium.base.task.ChainedTasks#add(int, Runnable)
        addRewriterTarget(
                Opcodes.INVOKEVIRTUAL,
                "org/chromium/base/task/ChainedTasks",
                "add",
                "(ILjava/lang/Runnable;)V",
                /* isInterface= */ false);
        // org.chromium.base.ThreadUtils#runOnUiThreadBlocking(Runnable)
        addRewriterTarget(
                Opcodes.INVOKESTATIC,
                "org/chromium/base/ThreadUtils",
                "runOnUiThreadBlocking",
                "(Ljava/lang/Runnable;)V",
                /* isInterface= */ false);
        // org.chromium.base.ThreadUtils#runOnUiThreadBlocking(Callable)
        addRewriterTarget(
                Opcodes.INVOKESTATIC,
                "org/chromium/base/ThreadUtils",
                "runOnUiThreadBlocking",
                "(Ljava/util/concurrent/Callable;)Ljava/lang/Object;",
                /* isInterface= */ false);
        // org.chromium.base.ThreadUtils#runOnUiThread(Runnable)
        addRewriterTarget(
                Opcodes.INVOKESTATIC,
                "org/chromium/base/ThreadUtils",
                "runOnUiThread",
                "(Ljava/lang/Runnable;)V",
                /* isInterface= */ false);
        // org.chromium.base.ThreadUtils#postOnUiThread(Runnable)
        addRewriterTarget(
                Opcodes.INVOKESTATIC,
                "org/chromium/base/ThreadUtils",
                "postOnUiThread",
                "(Ljava/lang/Runnable;)V",
                /* isInterface= */ false);
        // org.chromium.base.ThreadUtils#postOnUiThreadDelayed(Runnable, long)
        addRewriterTarget(
                Opcodes.INVOKESTATIC,
                "org/chromium/base/ThreadUtils",
                "postOnUiThreadDelayed",
                "(Ljava/lang/Runnable;J)V",
                /* isInterface= */ false);
        // org.chromium.base.task.AsyncTask#executeOnExecutor(LocationAwareExecutor)
        addRewriterTarget(
                Opcodes.INVOKEVIRTUAL,
                "org/chromium/base/task/AsyncTask",
                "executeOnExecutor",
                "(Lorg/chromium/base/task/LocationAwareExecutor;)Lorg/chromium/base/task/AsyncTask;",
                /* isInterface= */ false);
        // org.chromium.base.task.AsyncTask#executeOnTaskRunner(TaskRunner)
        addRewriterTarget(
                Opcodes.INVOKEVIRTUAL,
                "org/chromium/base/task/AsyncTask",
                "executeOnTaskRunner",
                "(Lorg/chromium/base/task/TaskRunner;)Lorg/chromium/base/task/AsyncTask;",
                /* isInterface= */ false);
        // org.chromium.base.task.AsyncTask#executeWithTaskTraits(int)
        addRewriterTarget(
                Opcodes.INVOKEVIRTUAL,
                "org/chromium/base/task/AsyncTask",
                "executeWithTaskTraits",
                "(I)Lorg/chromium/base/task/AsyncTask;",
                /* isInterface= */ false);
        // org.chromium.base.task.LocationAwareExecutor#execute(Runnable)
        addRewriterTarget(
                Opcodes.INVOKEINTERFACE,
                "org/chromium/base/task/LocationAwareExecutor",
                "execute",
                "(Ljava/lang/Runnable;)V",
                /* isInterface= */ true);
    }

    private static VisitableMethod getDefaultOverload(VisitableMethod method) {
        return new VisitableMethod(
                method.opcode,
                method.owner,
                method.name,
                method.descriptor.replace(")", LocationClass.DESCRIPTOR + ")"),
                method.isInterface);
    }

    private static void addRewriterTarget(
            int opcode, String owner, String name, String descriptor, boolean isInterface) {
        VisitableMethod target = new VisitableMethod(opcode, owner, name, descriptor, isInterface);
        OVERLOAD_MAP.put(target, getDefaultOverload(target));
    }

    private static boolean isExecuteAsyncTask(VisitableMethod method) {
        return ("executeOnExecutor".equals(method.name)
                        && "(Lorg/chromium/base/task/LocationAwareExecutor;)Lorg/chromium/base/task/AsyncTask;"
                                .equals(method.descriptor))
                || ("executeOnTaskRunner".equals(method.name)
                        && "(Lorg/chromium/base/task/TaskRunner;)Lorg/chromium/base/task/AsyncTask;"
                                .equals(method.descriptor))
                || ("executeWithTaskTraits".equals(method.name)
                        && "(I)Lorg/chromium/base/task/AsyncTask;".equals(method.descriptor));
    }

    public static VisitableMethod getOverload(VisitableMethod method) {
        VisitableMethod predefinedOverload = OVERLOAD_MAP.getOrDefault(method, null);
        if (predefinedOverload != null) {
            return predefinedOverload;
        }

        // TODO(anandrv): Dynamically generate the list of subclasses of AsyncTask once the rewriter
        // is moved to a later stage in the build pipeline and runs once per APK or module. For now,
        // there is only one method in the codebase that has this combination of method name and
        // descriptor so we can be confident that the method owner is a subclass of AsyncTask.
        if (isExecuteAsyncTask(method)) {
            return getDefaultOverload(method);
        }

        return null;
    }
}
