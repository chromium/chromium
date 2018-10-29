// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import static java.util.concurrent.TimeUnit.SECONDS;

import org.chromium.base.BuildConfig;
import org.chromium.base.VisibleForTesting;

import java.lang.reflect.Field;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

class ChromeThreadPoolExecutor extends ThreadPoolExecutor {
    private static final int CPU_COUNT = Runtime.getRuntime().availableProcessors();

    // Core pool is still used despite allowCoreThreadTimeOut(true) being called - while the core
    // pool can still timeout, the thread pool will still start up threads more aggressively while
    // under the CORE_POOL_SIZE.
    private static final int CORE_POOL_SIZE = Math.max(2, Math.min(CPU_COUNT - 1, 4));
    private static final int MAXIMUM_POOL_SIZE = CPU_COUNT * 2 + 1;
    private static final int KEEP_ALIVE_SECONDS = 30;

    private static final ThreadFactory sThreadFactory = new ThreadFactory() {
        private final AtomicInteger mCount = new AtomicInteger(1);
        @Override
        public Thread newThread(Runnable r) {
            return new Thread(r, "CrAsyncTask #" + mCount.getAndIncrement());
        }
    };

    private static final BlockingQueue<Runnable> sPoolWorkQueue =
            new ArrayBlockingQueue<Runnable>(128);

    // May have to be lowered if we are not capturing any Runnable sources.
    private static final int RUNNABLE_WARNING_COUNT = 32;

    ChromeThreadPoolExecutor() {
        this(CORE_POOL_SIZE, MAXIMUM_POOL_SIZE, KEEP_ALIVE_SECONDS, SECONDS, sPoolWorkQueue,
                sThreadFactory);
    }

    @VisibleForTesting
    ChromeThreadPoolExecutor(int corePoolSize, int maximumPoolSize, long keepAliveTime,
            TimeUnit unit, BlockingQueue<Runnable> workQueue, ThreadFactory threadFactory) {
        super(corePoolSize, maximumPoolSize, keepAliveTime, unit, workQueue, threadFactory);
        allowCoreThreadTimeOut(true);
    }

    @SuppressWarnings("NoAndroidAsyncTaskCheck")
    private static String getClassName(Runnable runnable) {
        Class blamedClass = runnable.getClass();
        try {
            if (blamedClass == AsyncTask.NamedFutureTask.class) {
                blamedClass = ((AsyncTask.NamedFutureTask) runnable).getBlamedClass();
            } else if (blamedClass.getEnclosingClass() == android.os.AsyncTask.class) {
                // This gets the AsyncTask that produced the runnable.
                Field field = blamedClass.getDeclaredField("this$0");
                field.setAccessible(true);
                blamedClass = field.get(runnable).getClass();
            }
        } catch (NoSuchFieldException e) {
            if (BuildConfig.DCHECK_IS_ON) {
                throw new RuntimeException(e);
            }
        } catch (IllegalAccessException e) {
            if (BuildConfig.DCHECK_IS_ON) {
                throw new RuntimeException(e);
            }
        }
        return blamedClass.getName();
    }

    private Map<String, Integer> getNumberOfClassNameOccurrencesInQueue() {
        Map<String, Integer> counts = new HashMap<>();
        Runnable[] copiedQueue = getQueue().toArray(new Runnable[0]);
        for (Runnable runnable : copiedQueue) {
            String className = getClassName(runnable);
            int count = counts.containsKey(className) ? counts.get(className) : 0;
            counts.put(className, count + 1);
        }
        return counts;
    }

    private String findClassNamesWithTooManyRunnables(Map<String, Integer> counts) {
        // We only show the classes over RUNNABLE_WARNING_COUNT appearances so that these
        // crashes group up together in the reporting dashboards. If we were to print all
        // the Runnables or their counts, this would fragment the reporting, with one for
        // each unique set of Runnables/counts.
        StringBuilder classesWithTooManyRunnables = new StringBuilder();
        for (Map.Entry<String, Integer> entry : counts.entrySet()) {
            if (entry.getValue() > RUNNABLE_WARNING_COUNT) {
                classesWithTooManyRunnables.append(entry.getKey()).append(' ');
            }
        }
        if (classesWithTooManyRunnables.length() == 0) {
            return "NO CLASSES FOUND";
        }
        return classesWithTooManyRunnables.toString();
    }

    @Override
    public void execute(Runnable command) {
        try {
            super.execute(command);
        } catch (RejectedExecutionException e) {
            Map<String, Integer> counts = getNumberOfClassNameOccurrencesInQueue();

            throw new RejectedExecutionException(
                    "Prominent classes in AsyncTask: " + findClassNamesWithTooManyRunnables(counts),
                    e);
        }
    }
}
