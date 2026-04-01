---
name: trace-analysis
description: Skill for analyzing trace files and finding hard causal links across threads and processes (using waker_utid).
---

## Skill: Trace Analysis for Blocked Threads

When analyzing traces for UI jank or blocked threads (e.g., monitor contention
on main thread):

1. **Don't just look at the ancestors of a slice**: Looking at ancestors only
   shows the calling hierarchy on that thread. It might lead to dead ends or
   vague links if the callback was posted or triggered by another event loop.
1. **Follow the 'Woken By' or 'Waking' metadata**: Look at the 'Running' or
   'Runnable' task on the timeline to inspect what event or thread woke it up or
   sent work to it. This bridges cross-thread and async gaps. In SQL queries,
   utilize the `waker_utid` and `waker_id` columns in the `thread_state` table
   to find the hard waker thread that moved the target from a sleeping to a
   runnable state.
1. **Look at syscalls to understand why a thread is not runnable**: If a thread
   is listed as `Sleeping` or `Blocked` (and not `Runnable`), look at recorded
   **syscalls** (such as `SYS_futex`) to understand what operation or system
   lock it was waiting for. This adds context on what exact kernel blocker
   stalled the thread.
1. **Trace cross-process flows (IPC/Binder)**: If a thread was woken by
   `system_server` or another process (like Play Store's `com.android.vending`),
   find out what service or operation was being called or completed (e.g.
   searching for `unbindServiceLocked` or `onAppUpdateInfo` in `slice` or `args`
   table). Using `waker_utid` to locate the waking thread allows identifying the
   exact operation handle on that waker thread, creating a causal chain across
   processes instead of relying on luck or timing estimates.
1. **Identify the chain of events**: Chrome -> Play Core library -> System
   Server (handling connection or binder completion) -> Event handled by
   background vs UI thread. This gives context on why a shared lock was taken
   and held synchronously.

This approach avoids getting stuck in opaque library calls and allows tracking
complete cross-process flows.
