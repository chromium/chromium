# Timing and Clocks on Android
There are several ways to measure time on Android. First, note the distinction between wall time and elapsed time.
Wall time is the user-visible presentation of the current moment, typically measured relative to some fixed epoch. This value can shift, jumping backwards or forwards unpredictably and should only be used when correspondence with real-world dates/times is crucial. If you do need this, just call System.currentTimeMillis().
Elapsed time, in contrast, is a measurement of the passage of time that is guaranteed to be monotonically non-decreasing. There is one important subdivision within elapsed time; this divides the timers into categories depending on which slices of "true" time count:
* elapsedRealTime and elapsedRealTimeNanos count everything, including deep sleep.
* uptimeMillis excludes deep sleep, e.g. when the device's CPU is off.
* currentThreadTimeMillis and threadCpuTimeNanos count only active CPU time in the current thread, ignoring e.g. blocking I/O.

This package contains some utility classes for measuring elapsed time with these different time sources. See below for more detailed guidance on when you should use each, but ElapsedRealTimeTimer is a good default.

# Scenarios
> "I want to record a metric that measures user visible time, including things like I/O and thread pre-emption"

Use ElapsedRealTimeTimer.
> "I want to record a metric that measures user visible time but not deep sleep."

Use UptimeTimer.
> "I just want to spot check how long a single thing takes locally; I don't need telemetry"

You can log the result of ElapsedRealTimeTimer.
> "I want to measure the performance of multiple methods that call eacher other."

You want TraceEvent.

> "I want to measure only elapsed CPU time for some potentially expensive operation, ignoring things like I/O and thread pre-emption. I'm positive that this operation is CPU-bound."

If you're absolutely sure about this, use CPUTimeTimer.

# Usage

```
Timer myTimer = new ElapsedRealTimeTimer();
myTimer.start();
// Measure elapsed time without stopping myTimer
long elapsedTimeMillis = myTimer.getElapsedTime(TimeUnit.MILLISECONDS);

// Get a finer or coarser granularity of elapsed time.
long elapsedTimeNanos = myTimer.getElapsedTime(TimeUnit.NANOSECONDS);
long elapsedTimeSeconds = myTimer.getElapsedTime(TimeUnit.SECONDS);

// Measure elapsed time after stopping myTimer. Repeated calls to getElapsedTime will return the same result.
myTimer.stop();
elapsedTimeMillis = myTimer.getElapsedTime(TimeUnit.MILLISECONDS);
Thread.sleep(1000);
assert elapsedTimeMillis == myTimer.getElapsedTime(TimeUnit.MILLISECONDS)

// Restart myTimer, resetting its start and stop times.
myTimer.start();
````
