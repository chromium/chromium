---
name: general/post-broadcastreceiver-registration-to-background-runner
rubric: safety
severity: should_fix
---

**Rule.** When registering or unregistering Android BroadcastReceivers where
immediate lockstep synchronization with the operating system is not required,
post the registration and unregistration calls asynchronously to a sequenced
background task runner (e.g., via PostTask.createSequencedTaskRunner) rather
than executing them synchronously on the UI thread. Use the same sequenced
runner for both operations to preserve registration and unregistration ordering.

**Why.** Android's registerReceiver() and unregisterReceiver() (and wrappers
like ContextUtils.registerProtectedBroadcastReceiver()) perform synchronous IPC
with the Android system server, which can introduce high tail latency and cause
UI jank. Offloading both operations to a single sequenced background task runner
keeps the UI thread responsive while guaranteeing consistent event ordering.

## Examples

Bad:

```cpp
private void register() {
    IntentFilter filter = new IntentFilter(Intent.ACTION_SCREEN_OFF);
    ContextUtils.registerProtectedBroadcastReceiver(
            ContextUtils.getApplicationContext(), this, filter);
}

private void unregister() {
    ContextUtils.getApplicationContext().unregisterReceiver(this);
}
```

Good:

```cpp
private final TaskRunner mTaskRunner =
        PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE);

private void register() {
    mTaskRunner.postDelayedTask(() -> {
        IntentFilter filter = new IntentFilter(Intent.ACTION_SCREEN_OFF);
        ContextUtils.registerProtectedBroadcastReceiver(
                ContextUtils.getApplicationContext(), this, filter);
    }, 0);
}

private void unregister() {
    // Unregister on the same sequenced task runner to ensure ordering.
    mTaskRunner.postDelayedTask(() -> {
        ContextUtils.getApplicationContext().unregisterReceiver(this);
    }, 0);
}
```
