# Nested Message Loop Mechanism

## Overview
In Chromium, the UI thread's message pump processes work in a round-robin
fashion from two primary sources: the Chrome Task Queue and the native OS
Message Queue (e.g., the Windows Message Queue). Bugs may occur when the thread
blocks and spins a nested message loop, causing re-entrancy into UI code that
destroys objects currently in use by the blocked outer stack frames.

## Mechanisms of Re-entrancy (Windows `SendMessage`)
On Windows, native API calls like `SetWindowPos` or `DestroyWindow` often
trigger synchronous `SendMessage` calls.

If a `SendMessage` call crosses thread boundaries, the sending thread blocks
until the target thread responds. While blocked, the OS will automatically
dispatch incoming **non-queued** messages to the waiting thread.

If the UI thread makes a synchronous cross-thread `SendMessage` call, the
sending thread is blocked until the receiving thread processes it. While
blocked, the OS will continue to dispatch incoming **non-queued** messages to
the waiting UI thread.
*   *Note:* Chrome tasks rely on *queued* messages, so they will not
    execute during this block. The risk comes entirely from native OS
    callbacks (e.g., window activation, focus changes, destruction)
    firing re-entrantly and calling back into Views code.
*   Messages already on the queue will not be dispatched during these calls.
    Only sending a message directly to the thread will dispatch immediately.
*   Any risk to these calls must originate from a direct `SendMessage` call to
    the HWND during a cross-thread call. When citing a direct `SendMessage`
    call as a risk, a clear code reference must be provided. `SendMessage`
    calls do not suddenly appear without a caller. In addition, Chrome cannot
    mitigate `SendMessage` calls from third-parties. These external callers are
    already running code inside the environment and can perform more damaging
    actions.
