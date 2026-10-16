---
name: general/catch-broad-exception-for-recoverable-android-os-apis
rubric: readability
severity: should_fix
---

**Rule.** When calling Android framework, OS, ContentResolver, or IPC APIs where
a safe fallback or error recovery path exists, catch broad `Exception`
(`catch (Exception e)`) rather than catching multiple specific unchecked
exception subclasses (such as `NullPointerException`,
`IllegalArgumentException`, `IllegalStateException`, `SecurityException`, or
`UnsupportedOperationException`). Consolidate redundant catch blocks that share
identical fallback handling, and do not catch `RuntimeException` or `Throwable`.

**Why.** Android OS services, remote ContentProviders, and OEM framework
implementations frequently throw arbitrary parcelable runtime or remote
exceptions across device models and OS versions. Catching narrow exception
subclasses leads to recurring production crashes when unhandled runtime
exceptions are thrown, while catching `Exception` ensures safe fallback. The
Chromium Java styleguide specifically advises `catch (Exception e)` over
`catch (RuntimeException e)` or `catch (Throwable t)` when recovering from OS
API failures.

## Examples

Bad:

```cpp
// Bad: Catching specific unchecked exception subclasses with duplicate fallback handling.
try {
    return context.getContentResolver().query(uri, projection, null, null, null);
} catch (NullPointerException e) {
    Log.w(TAG, "Cannot query provider for %s", uri, e);
    return null;
} catch (UnsupportedOperationException | IllegalArgumentException e) {
    Log.w(TAG, "Cannot query provider for %s", uri, e);
    return null;
}
```

Good:

```cpp
// Good: Catching broad Exception to handle arbitrary OS/provider runtime exceptions with safe fallback.
try {
    return context.getContentResolver().query(uri, projection, null, null, null);
} catch (Exception e) {
    Log.w(TAG, "Cannot query provider for %s", uri, e);
    return null;
}
```
