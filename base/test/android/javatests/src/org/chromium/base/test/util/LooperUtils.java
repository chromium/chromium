// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Test utilities for interacting with the Android Looper.
 */
public class LooperUtils {
    private static final Method sNextMethod = getMethod(MessageQueue.class, "next");
    private static final Field sMessageTargetField = getField(Message.class, "target");
    private static final Field sMessageFlagsField = getField(Message.class, "flags");

    private static Field getField(Class<?> clazz, String name) {
        Field f = null;
        try {
            f = clazz.getDeclaredField(name);
            f.setAccessible(true);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return f;
    }

    private static Method getMethod(Class<?> clazz, String name) {
        Method m = null;
        try {
            m = clazz.getDeclaredMethod(name);
            m.setAccessible(true);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return m;
    }

    /**
     * Runs a single nested task on the current Looper.
     */
    public static void runSingleNestedLooperTask() throws IllegalArgumentException,
                                                          IllegalAccessException, SecurityException,
                                                          InvocationTargetException {
        MessageQueue queue = Looper.myQueue();
        // This call will block if there are no messages in the queue. It will
        // also run or more pending C++ tasks as a side effect before returning
        // |msg|.
        Message msg = (Message) sNextMethod.invoke(queue);
        if (msg == null) return;
        Handler target = (Handler) sMessageTargetField.get(msg);

        if (target != null) target.dispatchMessage(msg);

        // Unset in-use flag.
        Integer oldFlags = (Integer) sMessageFlagsField.get(msg);
        sMessageFlagsField.set(msg, oldFlags & ~(1 << 0 /* FLAG_IN_USE */));

        msg.recycle();
    }
}
