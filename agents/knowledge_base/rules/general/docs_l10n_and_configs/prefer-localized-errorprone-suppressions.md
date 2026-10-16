---
name: general/prefer-localized-errorprone-suppressions
rubric: readability
severity: should_fix
---

**Rule.** Do not disable ErrorProne or static analysis checks target-wide in GN
build files (via `errorprone_args = [ "-Xep:<Check>:OFF" ]`) when only specific
files or methods require an exception. Scope exemptions locally using
`@SuppressWarnings("<Check>")` directly on the affected method, class, or field,
accompanied by an explanatory comment.

**Why.** Disabling an ErrorProne check target-wide in GN turns off static
analysis for every file in the target, allowing future bugs and regressions
across unrelated files to pass unnoticed. Local `@SuppressWarnings` annotations
restrict the exemption strictly to the necessary construct while keeping the
rest of the target protected.

## Examples

Bad:

```cpp
# In base/BUILD.gn:
android_library("base_java") {
  ...
  # TraceEvent.java uses Log.println() which is not available in org.chromium.base.Log.
  errorprone_args = [ "-Xep:NoAndroidLog:OFF" ]
}
```

Good:

```cpp
// In TraceEvent.java:
// Uses android.util.Log.println(), which is not available in org.chromium.base.Log.
@SuppressWarnings("NoAndroidLog")
private static void traceAndLog(int level, String message) {
  TraceEvent.instant("TraceEvent.LooperMonitor:IdleStats", message);
  android.util.Log.println(level, TAG, message);
}
```
