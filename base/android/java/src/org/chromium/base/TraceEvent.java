// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Activity;
import android.content.res.Resources.NotFoundException;
import android.os.Looper;
import android.os.MessageQueue;
import android.os.SystemClock;
import android.util.Log;
import android.util.Printer;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.ArrayList;

/**
 * Java mirror of Chrome trace event API. See base/trace_event/trace_event.h.
 *
 * To get scoped trace events, use the "try with resource" construct, for instance:
 * <pre>{@code
 * try (TraceEvent e = TraceEvent.scoped("MyTraceEvent")) {
 *   // code.
 * }
 * }</pre>
 *
 * The event name of the trace events must be a string literal or a |static final String| class
 * member. Otherwise NoDynamicStringsInTraceEventCheck error will be thrown.
 *
 * It is OK to use tracing before the native library has loaded, in a slightly restricted fashion.
 * @see EarlyTraceEvent for details.
 */
@JNINamespace("base::android")
public class TraceEvent implements AutoCloseable {
    private static volatile boolean sEnabled; // True when tracing into Chrome's tracing service.
    private static volatile boolean sUiThreadReady;
    private static boolean sEventNameFilteringEnabled;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static class BasicLooperMonitor implements Printer {
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        static final String LOOPER_TASK_PREFIX = "Looper.dispatch: ";

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        static final String FILTERED_EVENT_NAME = LOOPER_TASK_PREFIX + "EVENT_NAME_FILTERED";

        private static final int SHORTEST_LOG_PREFIX_LENGTH = "<<<<< Finished to ".length();
        private String mCurrentTarget;

        @Override
        public void println(final String line) {
            if (line.startsWith(">")) {
                beginHandling(line);
            } else {
                assert line.startsWith("<");
                endHandling(line);
            }
        }

        void beginHandling(final String line) {
            // May return an out-of-date value. this is not an issue as EarlyTraceEvent#begin()
            // will filter the event in this case.
            boolean earlyTracingActive = EarlyTraceEvent.enabled();
            if (sEnabled || earlyTracingActive) {
                // Note that we don't need to log ATrace events here because the
                // framework does that for us (M+).
                mCurrentTarget = getTraceEventName(line);
                if (sEnabled) {
                    TraceEventJni.get().beginToplevel(mCurrentTarget);
                } else {
                    EarlyTraceEvent.begin(mCurrentTarget, /* isToplevel= */ true);
                }
            }
        }

        void endHandling(final String line) {
            boolean earlyTracingActive = EarlyTraceEvent.enabled();
            if ((sEnabled || earlyTracingActive) && mCurrentTarget != null) {
                if (sEnabled) {
                    TraceEventJni.get().endToplevel();
                } else {
                    EarlyTraceEvent.end(mCurrentTarget, /* isToplevel= */ true);
                }
            }
            mCurrentTarget = null;
        }

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        static String getTraceEventName(String line) {
            if (sEventNameFilteringEnabled) {
                return FILTERED_EVENT_NAME;
            }
            return LOOPER_TASK_PREFIX + getTarget(line) + "(" + getTargetName(line) + ")";
        }

        /**
         * Android Looper formats |logLine| as
         *
         * ">>>>> Dispatching to (TARGET) {HASH_CODE} TARGET_NAME: WHAT"
         *
         * and
         *
         * "<<<<< Finished to (TARGET) {HASH_CODE} TARGET_NAME".
         *
         * This has been the case since at least 2009 (Donut). This function extracts the
         * TARGET part of the message.
         */
        private static String getTarget(String logLine) {
            int start = logLine.indexOf('(', SHORTEST_LOG_PREFIX_LENGTH);
            int end = start == -1 ? -1 : logLine.indexOf(')', start);
            return end != -1 ? logLine.substring(start + 1, end) : "";
        }

        // Extracts the TARGET_NAME part of the log message (see above).
        private static String getTargetName(String logLine) {
            int start = logLine.indexOf('}', SHORTEST_LOG_PREFIX_LENGTH);
            int end = start == -1 ? -1 : logLine.indexOf(':', start);
            if (end == -1) {
                end = logLine.length();
            }
            return start != -1 ? logLine.substring(start + 2, end) : "";
        }
    }

    /**
     * A class that records, traces and logs statistics about the UI thead's Looper.
     * The output of this class can be used in a number of interesting ways:
     * <p>
     * <ol><li>
     * When using chrometrace, there will be a near-continuous line of
     * measurements showing both event dispatches as well as idles;
     * </li><li>
     * Logging messages are output for events that run too long on the
     * event dispatcher, making it easy to identify problematic areas;
     * </li><li>
     * Statistics are output whenever there is an idle after a non-trivial
     * amount of activity, allowing information to be gathered about task
     * density and execution cadence on the Looper;
     * </li></ol>
     * <p>
     * The class attaches itself as an idle handler to the main Looper, and
     * monitors the execution of events and idle notifications. Task counters
     * accumulate between idle notifications and get reset when a new idle
     * notification is received.
     */
    private static final class IdleTracingLooperMonitor extends BasicLooperMonitor
            implements MessageQueue.IdleHandler {
        // Tags for dumping to logcat or TraceEvent
        private static final String TAG = "TraceEvt_LooperMonitor";
        private static final String IDLE_EVENT_NAME = "Looper.queueIdle";

        // Calculation constants
        private static final long FRAME_DURATION_MILLIS = 1000L / 60L; // 60 FPS
        // A reasonable threshold for defining a Looper event as "long running"
        private static final long MIN_INTERESTING_DURATION_MILLIS = FRAME_DURATION_MILLIS;
        // A reasonable threshold for a "burst" of tasks on the Looper
        private static final long MIN_INTERESTING_BURST_DURATION_MILLIS =
                MIN_INTERESTING_DURATION_MILLIS * 3;

        // Stats tracking
        private long mLastIdleStartedAt;
        private long mLastWorkStartedAt;
        private int mNumTasksSeen;
        private int mNumIdlesSeen;
        private int mNumTasksSinceLastIdle;

        // State
        private boolean mIdleMonitorAttached;

        // Called from within the begin/end methods only.
        // This method can only execute on the looper thread, because that is
        // the only thread that is permitted to call Looper.myqueue().
        private final void syncIdleMonitoring() {
            if (sEnabled && !mIdleMonitorAttached) {
                // approximate start time for computational purposes
                mLastIdleStartedAt = TimeUtils.elapsedRealtimeMillis();
                Looper.myQueue().addIdleHandler(this);
                mIdleMonitorAttached = true;
                Log.v(TAG, "attached idle handler");
            } else if (mIdleMonitorAttached && !sEnabled) {
                Looper.myQueue().removeIdleHandler(this);
                mIdleMonitorAttached = false;
                Log.v(TAG, "detached idle handler");
            }
        }

        @Override
        final void beginHandling(final String line) {
            // Close-out any prior 'idle' period before starting new task.
            if (mNumTasksSinceLastIdle == 0) {
                TraceEvent.end(IDLE_EVENT_NAME);
            }
            mLastWorkStartedAt = TimeUtils.elapsedRealtimeMillis();
            syncIdleMonitoring();
            super.beginHandling(line);
        }

        @Override
        final void endHandling(final String line) {
            final long elapsed = TimeUtils.elapsedRealtimeMillis() - mLastWorkStartedAt;
            if (elapsed > MIN_INTERESTING_DURATION_MILLIS) {
                traceAndLog(Log.WARN, "observed a task that took " + elapsed + "ms: " + line);
            }
            super.endHandling(line);
            syncIdleMonitoring();
            mNumTasksSeen++;
            mNumTasksSinceLastIdle++;
        }

        private static void traceAndLog(int level, String message) {
            TraceEvent.instant("TraceEvent.LooperMonitor:IdleStats", message);
            Log.println(level, TAG, message);
        }

        @Override
        public final boolean queueIdle() {
            final long now = TimeUtils.elapsedRealtimeMillis();
            if (mLastIdleStartedAt == 0) mLastIdleStartedAt = now;
            final long elapsed = now - mLastIdleStartedAt;
            mNumIdlesSeen++;
            TraceEvent.begin(IDLE_EVENT_NAME, mNumTasksSinceLastIdle + " tasks since last idle.");
            if (elapsed > MIN_INTERESTING_BURST_DURATION_MILLIS) {
                // Dump stats
                String statsString =
                        mNumTasksSeen
                                + " tasks and "
                                + mNumIdlesSeen
                                + " idles processed so far, "
                                + mNumTasksSinceLastIdle
                                + " tasks bursted and "
                                + elapsed
                                + "ms elapsed since last idle";
                traceAndLog(Log.DEBUG, statsString);
            }
            mLastIdleStartedAt = now;
            mNumTasksSinceLastIdle = 0;
            return true; // stay installed
        }
    }

    // Holder for monitor avoids unnecessary construction on non-debug runs
    private static final class LooperMonitorHolder {
        private static final BasicLooperMonitor sInstance =
                CommandLine.getInstance().hasSwitch(BaseSwitches.ENABLE_IDLE_TRACING)
                        ? new IdleTracingLooperMonitor()
                        : new BasicLooperMonitor();
    }

    private final String mName;

    /** Constructor used to support the "try with resource" construct. */
    private TraceEvent(String name, String arg) {
        mName = name;
        begin(name, arg);
    }

    /** Constructor used to support the "try with resource" construct. */
    private TraceEvent(String name, int arg) {
        mName = name;
        begin(name, arg);
    }

    @Override
    public void close() {
        end(mName);
    }

    /**
     * Factory used to support the "try with resource" construct.
     *
     * Note that if tracing is not enabled, this will not result in allocating an object.
     *
     * @param name Trace event name.
     * @param arg The arguments of the event.
     * @return a TraceEvent, or null if tracing is not enabled.
     */
    public static TraceEvent scoped(String name, String arg) {
        if (!(EarlyTraceEvent.enabled() || enabled())) return null;
        return new TraceEvent(name, arg);
    }

    /**
     * Factory used to support the "try with resource" construct.
     *
     * Note that if tracing is not enabled, this will not result in allocating an object.
     *
     * @param name Trace event name.
     * @param arg An integer argument of the event.
     * @return a TraceEvent, or null if tracing is not enabled.
     */
    public static TraceEvent scoped(String name, int arg) {
        if (!(EarlyTraceEvent.enabled() || enabled())) return null;
        return new TraceEvent(name, arg);
    }

    /** Similar to {@link #scoped(String, String arg)}, but uses null for |arg|. */
    public static TraceEvent scoped(String name) {
        return scoped(name, null);
    }

    /** Notification from native that tracing is enabled/disabled. */
    @CalledByNative
    public static void setEnabled(boolean enabled) {
        if (enabled) EarlyTraceEvent.disable();
        // Only disable logging if Chromium enabled it originally, so as to not disrupt logging done
        // by other applications
        if (sEnabled != enabled) {
            sEnabled = enabled;
            ThreadUtils.getUiThreadLooper()
                    .setMessageLogging(enabled ? LooperMonitorHolder.sInstance : null);
        }

        if (sEnabled) {
            EarlyTraceEvent.dumpActivityStartupEvents();
        }

        if (sUiThreadReady) {
            ViewHierarchyDumper.updateEnabledState();
        }
    }

    @CalledByNative
    public static void setEventNameFilteringEnabled(boolean enabled) {
        sEventNameFilteringEnabled = enabled;
    }

    public static boolean eventNameFilteringEnabled() {
        return sEventNameFilteringEnabled;
    }

    /**
     * May enable early tracing depending on the environment.
     *
     * @param readCommandLine If true, also check command line flags to see
     *                        whether tracing should be turned on.
     */
    public static void maybeEnableEarlyTracing(boolean readCommandLine) {
        // Enable early trace events based on command line flags. This is only
        // done for Chrome since WebView tracing isn't controlled with command
        // line flags.
        if (readCommandLine) {
            EarlyTraceEvent.maybeEnableInBrowserProcess();
        }
        if (EarlyTraceEvent.enabled()) {
            ThreadUtils.getUiThreadLooper().setMessageLogging(LooperMonitorHolder.sInstance);
        }
    }

    public static void onNativeTracingReady() {
        TraceEventJni.get().registerEnabledObserver();
    }

    // Called by ThreadUtils.
    static void onUiThreadReady() {
        sUiThreadReady = true;
        if (sEnabled) {
            ViewHierarchyDumper.updateEnabledState();
        }
    }

    /**
     * @return True if tracing is enabled, false otherwise.
     * It is safe to call trace methods without checking if TraceEvent
     * is enabled.
     */
    public static boolean enabled() {
        return sEnabled;
    }

    /**
     * Triggers the 'instant' native trace event with no arguments.
     * @param name The name of the event.
     */
    public static void instant(String name) {
        if (sEnabled) TraceEventJni.get().instant(name, null);
    }

    /**
     * Triggers the 'instant' native trace event.
     * @param name The name of the event.
     * @param arg  The arguments of the event.
     */
    public static void instant(String name, String arg) {
        if (sEnabled) TraceEventJni.get().instant(name, arg);
    }

    /**
     * Triggers a 'instant' native "AndroidIPC" event.
     * @param name The name of the IPC.
     * @param durMs The duration the IPC took in milliseconds.
     */
    public static void instantAndroidIPC(String name, long durMs) {
        if (sEnabled) TraceEventJni.get().instantAndroidIPC(name, durMs);
    }

    /**
     * Triggers a 'instant' native "AndroidToolbar" event.
     * @param blockReason the enum TopToolbarBlockCapture (-1 if not blocked).
     * @param allowReason the enum TopToolbarAllowCapture (-1 if not allowed).
     * @param snapshotDiff the enum ToolbarSnapshotDifference (-1 if no diff).
     */
    public static void instantAndroidToolbar(int blockReason, int allowReason, int snapshotDiff) {
        if (sEnabled) {
            TraceEventJni.get().instantAndroidToolbar(blockReason, allowReason, snapshotDiff);
        }
    }

    /**
     * Records a 'WebView.Startup.CreationTime.TotalFactoryInitTime' event with the
     * 'android_webview.timeline' category starting at `startTimeMs` with the duration of
     * `durationMs`.
     */
    public static void webViewStartupTotalFactoryInit(long startTimeMs, long durationMs) {
        if (sEnabled) {
            TraceEventJni.get().webViewStartupTotalFactoryInit(startTimeMs, durationMs);
        }
    }

    /**
     * Records a 'WebView.Startup.CreationTime.Stage1.FactoryInit' event with the
     * 'android_webview.timeline' category starting at `startTimeMs` with the duration of
     * `durationMs`.
     */
    public static void webViewStartupStage1(long startTimeMs, long durationMs) {
        if (sEnabled) {
            TraceEventJni.get().webViewStartupStage1(startTimeMs, durationMs);
        }
    }

    /**
     * Records 'WebView.Startup.CreationTime.Stage2.ProviderInit.Warm' and
     * 'WebView.Startup.CreationTime.Stage2.ProviderInit.Cold' events depending on the value of
     * `isColdStartup` with the 'android_webview.timeline' category starting at `startTimeMs` with
     * the duration of `durationMs`.
     */
    public static void webViewStartupStage2(
            long startTimeMs, long durationMs, boolean isColdStartup) {
        if (sEnabled) {
            TraceEventJni.get().webViewStartupStage2(startTimeMs, durationMs, isColdStartup);
        }
    }

    /**
     * Records a 'WebView.Startup.CreationTime.StartChromiumLocked' event with the
     * 'android_webview.timeline' category starting at `startTimeMs` with the duration of
     * `durationMs`. `callSite` and `fromUIThread` are set as the arguments for the event.
     */
    public static void webViewStartupStartChromiumLocked(
            long startTimeMs, long durationMs, int callSite, boolean fromUIThread) {
        if (sEnabled) {
            TraceEventJni.get()
                    .webViewStartupStartChromiumLocked(
                            startTimeMs, durationMs, callSite, fromUIThread);
        }
    }

    /** Records 'Startup.ActivityStart' event with the 'interactions' category. */
    public static void startupActivityStart(long activityId, long startTimeMs) {
        if (sEnabled) {
            TraceEventJni.get().startupActivityStart(activityId, startTimeMs);
        } else {
            EarlyTraceEvent.startupActivityStart(activityId, startTimeMs);
        }
    }

    /** Records 'Startup.LaunchCause' event with the 'interactions' category. */
    public static void startupLaunchCause(long activityId, int launchCause) {
        if (sEnabled) {
            TraceEventJni.get()
                    .startupLaunchCause(activityId, SystemClock.uptimeMillis(), launchCause);
        } else {
            EarlyTraceEvent.startupLaunchCause(activityId, launchCause);
        }
    }

    /** Records 'Startup.TimeToFirstVisibleContent2' event with the 'interactions' category. */
    public static void startupTimeToFirstVisibleContent2(
            long activityId, long startTimeMs, long durationMs) {
        if (!sEnabled) return;
        TraceEventJni.get().startupTimeToFirstVisibleContent2(activityId, startTimeMs, durationMs);
    }

    /**
     * Snapshots the view hierarchy state on the main thread and then finishes emitting a trace
     * event on the threadpool.
     */
    public static void snapshotViewHierarchy() {
        if (sEnabled && TraceEventJni.get().viewHierarchyDumpEnabled()) {
            // Emit separate begin and end so we can set the flow id at the end.
            TraceEvent.begin("instantAndroidViewHierarchy");

            // If we have no views don't bother to emit any TraceEvents for efficiency.
            ArrayList<ActivityInfo> views = snapshotViewHierarchyState();
            if (views.isEmpty()) {
                TraceEvent.end("instantAndroidViewHierarchy");
                return;
            }

            // Use the correct snapshot object as a processed scoped flow id. This connects the
            // mainthread work with the result emitted on the threadpool. We do this because
            // resolving resource names can trigger exceptions (NotFoundException) which can be
            // quite slow.
            long flow = views.hashCode();

            PostTask.postTask(
                    TaskTraits.BEST_EFFORT,
                    () -> {
                        // Actually output the dump as a trace event on a thread pool.
                        TraceEventJni.get().initViewHierarchyDump(flow, views);
                    });
            TraceEvent.end("instantAndroidViewHierarchy", null, flow);
        }
    }

    /**
     * Triggers the 'start' native trace event with no arguments.
     * @param name The name of the event.
     * @param id   The id of the asynchronous event.
     */
    public static void startAsync(String name, long id) {
        EarlyTraceEvent.startAsync(name, id);
        if (sEnabled) {
            TraceEventJni.get().startAsync(name, id);
        }
    }

    /**
     * Triggers the 'finish' native trace event with no arguments.
     * @param name The name of the event.
     * @param id   The id of the asynchronous event.
     */
    public static void finishAsync(String name, long id) {
        EarlyTraceEvent.finishAsync(name, id);
        if (sEnabled) {
            TraceEventJni.get().finishAsync(id);
        }
    }

    /**
     * Triggers the 'begin' native trace event with no arguments.
     * @param name The name of the event.
     */
    public static void begin(String name) {
        begin(name, null);
    }

    /**
     * Triggers the 'begin' native trace event.
     * @param name The name of the event.
     * @param arg  The arguments of the event.
     */
    public static void begin(String name, String arg) {
        EarlyTraceEvent.begin(name, /* isToplevel= */ false);
        if (sEnabled) {
            TraceEventJni.get().begin(name, arg);
        }
    }

    /**
     * Triggers the 'begin' native trace event.
     * @param name The name of the event.
     * @param arg An integer argument of the event.
     */
    public static void begin(String name, int arg) {
        EarlyTraceEvent.begin(name, /* isToplevel= */ false);
        if (sEnabled) {
            TraceEventJni.get().beginWithIntArg(name, arg);
        }
    }

    /**
     * Triggers the 'end' native trace event with no arguments.
     * @param name The name of the event.
     */
    public static void end(String name) {
        end(name, null);
    }

    /**
     * Triggers the 'end' native trace event.
     * @param name The name of the event.
     * @param arg  The arguments of the event.
     */
    public static void end(String name, String arg) {
        end(name, arg, 0);
    }

    /**
     * Triggers the 'end' native trace event.
     * @param name The name of the event.
     * @param arg  The arguments of the event.
     * @param flow The flow ID to associate with this event (0 is treated as invalid).
     */
    public static void end(String name, String arg, long flow) {
        EarlyTraceEvent.end(name, /* isToplevel= */ false);
        if (sEnabled) {
            TraceEventJni.get().end(arg, flow);
        }
    }

    public static ArrayList<ActivityInfo> snapshotViewHierarchyState() {
        if (!ApplicationStatus.isInitialized()) {
            return new ArrayList<ActivityInfo>();
        }

        // In local testing we generally just have one activity.
        ArrayList<ActivityInfo> views = new ArrayList<>(2);
        for (Activity a : ApplicationStatus.getRunningActivities()) {
            views.add(new ActivityInfo(a.getClass().getName()));
            ViewHierarchyDumper.dumpView(
                    views.get(views.size() - 1),
                    /* parentId= */ 0,
                    a.getWindow().getDecorView().getRootView());
        }
        return views;
    }

    @NativeMethods
    interface Natives {
        void registerEnabledObserver();

        void instant(String name, String arg);

        void begin(String name, String arg);

        void beginWithIntArg(String name, int arg);

        void end(String arg, long flow);

        void beginToplevel(String target);

        void endToplevel();

        void startAsync(String name, long id);

        void finishAsync(long id);

        boolean viewHierarchyDumpEnabled();

        void initViewHierarchyDump(long id, Object list);

        long startActivityDump(String name, long dumpProtoPtr);

        void addViewDump(
                int id,
                int parentId,
                boolean isShown,
                boolean isDirty,
                String className,
                String resourceName,
                long activityProtoPtr);

        void instantAndroidIPC(String name, long durMs);

        void instantAndroidToolbar(int blockReason, int allowReason, int snapshotDiff);

        void webViewStartupTotalFactoryInit(long startTimeMs, long durationMs);

        void webViewStartupStage1(long startTimeMs, long durationMs);

        void webViewStartupStage2(long startTimeMs, long durationMs, boolean isColdStartup);

        void webViewStartupStartChromiumLocked(
                long startTimeMs, long durationMs, int callSite, boolean fromUIThread);

        void startupActivityStart(long activityId, long startTimeMs);

        void startupLaunchCause(long activityId, long startTimeMs, int launchCause);

        void startupTimeToFirstVisibleContent2(long activityId, long startTimeMs, long durationMs);
    }

    /**
     * A method to be called by native code that uses the ViewHierarchyDumper class to emit a trace
     * event with views of all running activities of the app.
     */
    @CalledByNative
    public static void dumpViewHierarchy(long dumpProtoPtr, Object list) {
        if (!ApplicationStatus.isInitialized()) {
            return;
        }

        // Convert the Object back into the ArrayList of ActivityInfo, lifetime of this object is
        // maintained by the Runnable that we are running in currently.
        ArrayList<ActivityInfo> activities = (ArrayList<ActivityInfo>) list;

        for (ActivityInfo activity : activities) {
            long activityProtoPtr =
                    TraceEventJni.get().startActivityDump(activity.mActivityName, dumpProtoPtr);
            for (ViewInfo view : activity.mViews) {
                // We need to resolve the resource, take care as NotFoundException can be common and
                // java exceptions aren't he fastest thing ever.
                String resource;
                try {
                    resource =
                            view.mRes != null
                                    ? (view.mId == 0 || view.mId == -1
                                            ? "__no_id__"
                                            : view.mRes.getResourceName(view.mId))
                                    : "__no_resources__";
                } catch (NotFoundException e) {
                    resource = "__name_not_found__";
                }
                TraceEventJni.get()
                        .addViewDump(
                                view.mId,
                                view.mParentId,
                                view.mIsShown,
                                view.mIsDirty,
                                view.mClassName,
                                resource,
                                activityProtoPtr);
            }
        }
    }

    /**
     * This class contains the minimum information to represent a view that the {@link
     * #ViewHierarchyDumper} needs, so that in {@link #snapshotViewHierarchy} we can output a trace
     * event off the main thread.
     */
    public static class ViewInfo {
        public ViewInfo(
                int id,
                int parentId,
                boolean isShown,
                boolean isDirty,
                String className,
                android.content.res.Resources res) {
            mId = id;
            mParentId = parentId;
            mIsShown = isShown;
            mIsDirty = isDirty;
            mClassName = className;
            mRes = res;
        }

        private int mId;
        private int mParentId;
        private boolean mIsShown;
        private boolean mIsDirty;
        private String mClassName;
        // One can use mRes to resolve mId to a resource name.
        private android.content.res.Resources mRes;
    }

    /**
     * This class contains the minimum information to represent an Activity that the {@link
     * #ViewHierarchyDumper} needs, so that in {@link #snapshotViewHierarchy} we can output a trace
     * event off the main thread.
     */
    public static class ActivityInfo {
        public ActivityInfo(String activityName) {
            mActivityName = activityName;
            // Local testing found about 115ish views in the ChromeTabbedActivity.
            mViews = new ArrayList<ViewInfo>(125);
        }

        public String mActivityName;
        public ArrayList<ViewInfo> mViews;
    }

    /**
     * A class that periodically dumps the view hierarchy of all running activities of the app to
     * the trace. Enabled/disabled via the disabled-by-default-android_view_hierarchy trace
     * category.
     *
     * <pre>
     * The class registers itself as an idle handler, so that it can run when there are no other
     * tasks in the queue (but not more often than once a second). When the queue is idle,
     * it calls the initViewHierarchyDump() native function which in turn calls the
     * TraceEvent.dumpViewHierarchy() with a pointer to the proto buffer to fill in. The
     * TraceEvent.dumpViewHierarchy() traverses all activities and dumps view hierarchy for every
     * activity. Altogether, the call sequence is as follows:
     *   ViewHierarchyDumper.queueIdle()
     *    -> JNI#initViewHierarchyDump()
     *        -> TraceEvent.dumpViewHierarchy()
     *            -> JNI#startActivityDump()
     *            -> ViewHierarchyDumper.dumpView()
     *                -> JNI#addViewDump()
     * </pre>
     */
    private static final class ViewHierarchyDumper implements MessageQueue.IdleHandler {
        private static final long MIN_VIEW_DUMP_INTERVAL_MILLIS = 1000L;
        private static boolean sEnabled;
        private static ViewHierarchyDumper sInstance;
        private long mLastDumpTs;

        @Override
        public final boolean queueIdle() {
            final long now = TimeUtils.elapsedRealtimeMillis();
            if (mLastDumpTs == 0 || (now - mLastDumpTs) > MIN_VIEW_DUMP_INTERVAL_MILLIS) {
                mLastDumpTs = now;
                snapshotViewHierarchy();
            }

            // Returning true to keep IdleHandler alive.
            return true;
        }

        public static void updateEnabledState() {
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        if (TraceEventJni.get().viewHierarchyDumpEnabled()) {
                            if (sInstance == null) {
                                sInstance = new ViewHierarchyDumper();
                            }
                            enable();
                        } else {
                            if (sInstance != null) {
                                disable();
                            }
                        }
                    });
        }

        private static void dumpView(ActivityInfo collection, int parentId, View v) {
            ThreadUtils.assertOnUiThread();
            int id = v.getId();
            collection.mViews.add(
                    new ViewInfo(
                            id,
                            parentId,
                            v.isShown(),
                            v.isDirty(),
                            v.getClass().getSimpleName(),
                            v.getResources()));

            if (v instanceof ViewGroup) {
                ViewGroup vg = (ViewGroup) v;
                for (int i = 0; i < vg.getChildCount(); i++) {
                    dumpView(collection, id, vg.getChildAt(i));
                }
            }
        }

        private static void enable() {
            ThreadUtils.assertOnUiThread();
            if (!sEnabled) {
                Looper.myQueue().addIdleHandler(sInstance);
                sEnabled = true;
            }
        }

        private static void disable() {
            ThreadUtils.assertOnUiThread();
            if (sEnabled) {
                Looper.myQueue().removeIdleHandler(sInstance);
                sEnabled = false;
            }
        }
    }
}
