// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.annotation.SuppressLint;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;

import java.lang.ref.ReferenceQueue;
import java.lang.ref.WeakReference;
import java.util.HashSet;
import java.util.Set;

/**
 * Handles running cleanup tasks when an object becomes eligible for GC. Cleanup tasks
 * are always executed on the main thread. In general, classes should not have
 * finalizers and likewise should not use this class for the same reasons. The
 * exception is where public APIs exist that require native side resources to be
 * cleaned up in response to java side GC of API objects. (Private/internal
 * interfaces should always favor explicit resource releases / destroy()
 * protocol for this rather than depend on GC to trigger native cleanup).
 * NOTE this uses WeakReference rather than PhantomReference, to avoid delaying the
 * cleanup processing until after finalizers (if any) have run. In general usage of
 * this class indicates the client does NOT use finalizers anyway (Good), so this should
 * not be a visible difference in practice.
 */
public class CleanupReference extends WeakReference<Object> {
    private static final String TAG = "CleanupReference";

    private static final boolean DEBUG = false; // Always check in as false!

    // The VM will enqueue CleanupReference instance onto sGcQueue when it becomes eligible for
    // garbage collection (i.e. when all references to the underlying object are nullified).
    // |sReaperThread| processes this queue by forwarding the references on to the UI thread
    // (via REMOVE_REF message) to perform cleanup.
    private static ReferenceQueue<Object> sGcQueue = new ReferenceQueue<Object>();
    private static Object sCleanupMonitor = new Object();

    private static final Thread sReaperThread =
            new Thread(TAG) {
                @Override
                @SuppressWarnings("WaitNotInLoop")
                public void run() {
                    while (true) {
                        try {
                            CleanupReference ref = (CleanupReference) sGcQueue.remove();
                            if (DEBUG) Log.d(TAG, "removed one ref from GC queue");
                            synchronized (sCleanupMonitor) {
                                Message.obtain(LazyHolder.sHandler, REMOVE_REF, ref).sendToTarget();
                                // Give the UI thread chance to run cleanup before looping around
                                // and taking the next item from the queue, to avoid Message
                                // bombing it.
                                sCleanupMonitor.wait(500);
                            }
                        } catch (Exception e) {
                            Log.e(TAG, "Queue remove exception:", e);
                        }
                    }
                }
            };

    static {
        sReaperThread.setDaemon(true);
        sReaperThread.start();
    }

    // Message's sent in the |what| field to |sHandler|.

    // Add a new reference to sRefs. |msg.obj| is the CleanupReference to add.
    private static final int ADD_REF = 1;
    // Remove reference from sRefs. |msg.obj| is the CleanupReference to remove.
    private static final int REMOVE_REF = 2;

    /**
     * This {@link Handler} polls {@link #sRefs}, looking for cleanup tasks that
     * are ready to run.
     * This is lazily initialized as ThreadUtils.getUiThreadLooper() may not be
     * set yet early in startup.
     */
    @SuppressLint("HandlerLeak")
    private static class LazyHolder {
        static final Handler sHandler =
                new Handler(ThreadUtils.getUiThreadLooper()) {
                    @Override
                    public void handleMessage(Message msg) {
                        try {
                            TraceEvent.begin("CleanupReference.LazyHolder.handleMessage");
                            CleanupReference ref = (CleanupReference) msg.obj;
                            switch (msg.what) {
                                case ADD_REF:
                                    sRefs.add(ref);
                                    break;
                                case REMOVE_REF:
                                    ref.runCleanupTaskInternal();
                                    break;
                                default:
                                    Log.e(TAG, "Bad message=%d", msg.what);
                                    break;
                            }

                            if (DEBUG) Log.d(TAG, "will try and cleanup; max = %d", sRefs.size());

                            synchronized (sCleanupMonitor) {
                                // Always run the cleanup loop here even when adding or removing
                                // refs, to avoid falling behind on rapid garbage allocation inner
                                // loops.
                                while ((ref = (CleanupReference) sGcQueue.poll()) != null) {
                                    ref.runCleanupTaskInternal();
                                }
                                sCleanupMonitor.notifyAll();
                            }
                        } finally {
                            TraceEvent.end("CleanupReference.LazyHolder.handleMessage");
                        }
                    }
                };
    }

    /**
     * Keep a strong reference to {@link CleanupReference} so that it will
     * actually get enqueued.
     * Only accessed on the UI thread.
     */
    private static Set<CleanupReference> sRefs = new HashSet<CleanupReference>();

    private Runnable mCleanupTask;

    /**
     * @param obj the object whose loss of reachability should trigger the
     *            cleanup task.
     * @param cleanupTask the task to run once obj loses reachability.
     */
    public CleanupReference(Object obj, Runnable cleanupTask) {
        super(obj, sGcQueue);
        if (DEBUG) Log.d(TAG, "+++ CREATED ONE REF");
        mCleanupTask = cleanupTask;
        handleOnUiThread(ADD_REF);
    }

    /**
     * Clear the cleanup task {@link Runnable} so that nothing will be done
     * after garbage collection.
     */
    public void cleanupNow() {
        handleOnUiThread(REMOVE_REF);
    }

    public boolean hasCleanedUp() {
        return mCleanupTask == null;
    }

    private void handleOnUiThread(int what) {
        Message msg = Message.obtain(LazyHolder.sHandler, what, this);
        if (Looper.myLooper() == msg.getTarget().getLooper()) {
            msg.getTarget().handleMessage(msg);
            msg.recycle();
        } else {
            msg.sendToTarget();
        }
    }

    private void runCleanupTaskInternal() {
        if (DEBUG) Log.d(TAG, "runCleanupTaskInternal");
        sRefs.remove(this);
        Runnable cleanupTask = mCleanupTask;
        mCleanupTask = null;
        if (cleanupTask != null) {
            if (DEBUG) Log.i(TAG, "--- CLEANING ONE REF");
            cleanupTask.run();
        }
        clear();
    }
}
