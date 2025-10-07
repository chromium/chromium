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
    }

    private static void addRewriterTarget(
            int opcode, String owner, String name, String descriptor, boolean isInterface) {
        VisitableMethod target = new VisitableMethod(opcode, owner, name, descriptor, isInterface);
        VisitableMethod overload =
                new VisitableMethod(
                        opcode,
                        owner,
                        name,
                        descriptor.replace(")", LocationClass.DESCRIPTOR + ")"),
                        isInterface);
        OVERLOAD_MAP.put(target, overload);
    }

    public static VisitableMethod getOverload(VisitableMethod method) {
        return OVERLOAD_MAP.getOrDefault(method, null);
    }
}
