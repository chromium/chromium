package com.ark.browser.utils;

import android.os.Build;
import android.os.Handler;
import android.os.Looper;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

public class ThreadPool {

    private static final int CPU_COUNT = Runtime.getRuntime().availableProcessors();

    private static final ExecutorService EXECUTOR = new ThreadPoolExecutor(
            0, Integer.MAX_VALUE,
            30L, TimeUnit.SECONDS,
            new SynchronousQueue<>()
    );

    private static final class Holder {
        private static final Handler HANDLER = new Handler(Looper.getMainLooper());
    }

    private static final class IOExecutorHolder {
        private static final int THREAD_COUNT = 2 * CPU_COUNT + 1;
        private static final ThreadPoolExecutor EXECUTOR = new ThreadPoolExecutor(
                THREAD_COUNT, THREAD_COUNT,
                0L, TimeUnit.MILLISECONDS,
                new LinkedBlockingQueue<>(),
                r -> new Thread(r, "IOThreadPool")
        );
    }

    public static void execute(Runnable runnable) {
        EXECUTOR.execute(runnable);
    }

    public static void executeIO(Runnable runnable) {
//        if (!IOExecutorHolder.EXECUTOR.allowsCoreThreadTimeOut()) {
//            IOExecutorHolder.EXECUTOR.allowCoreThreadTimeOut(true);
//        }
        IOExecutorHolder.EXECUTOR.execute(runnable);
    }

    public static void runOnUIThread(Runnable runnable) {
        if (runnable == null) {
            return;
        }
        if (Looper.myLooper() == Looper.getMainLooper()) {
            runnable.run();
        } else {
            Holder.HANDLER.post(runnable);
        }
    }

    public static void postOnUIThread(Runnable runnable) {
        if (runnable == null) {
            return;
        }
        Holder.HANDLER.post(runnable);
    }

    public static void postIdle(Runnable runnable) {
        if (runnable == null) {
            return;
        }
        if (Looper.myLooper() == Looper.getMainLooper()) {
            Looper.myQueue().addIdleHandler(() -> {
                runnable.run();
                return false;
            });
        } else {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                Holder.HANDLER.getLooper().getQueue().addIdleHandler(() -> {
                    runnable.run();
                    return false;
                });
            } else {
                Holder.HANDLER.post(runnable);
            }
        }
    }

    public static void postDelayed(Runnable r, long delayMillis) {
        if (r == null) {
            return;
        }
        Holder.HANDLER.postDelayed(r, delayMillis);
    }

    public static void removeCallbacks(Runnable r) {
        if (r == null) {
            return;
        }
        Holder.HANDLER.removeCallbacks(r);
    }

    public static Future<?> submit(Runnable runnable) {
        return EXECUTOR.submit(runnable);
    }

}
