---
name: general/bypass-strict-mode-context-for-r8-optimized-operations
rubric: safety
severity: blocking
---

**Rule.** When suppressing StrictMode violations for operations (such as
ServiceLoader.load or reflection) that only trigger violations in debug builds
and are optimized away by R8 at build time, bypass StrictModeContext in release
builds using `if (BuildConfig.IS_PROGUARD_ENABLED)`. Do not unconditionally wrap
build-time-optimized operations in `StrictModeContext` try-with-resources
blocks.

**Why.** In debug builds, operations like ServiceLoader.load perform reflection
and disk reads that trigger StrictMode violations, but R8 optimizes these calls
away into direct instantiations in release builds. While Chrome release builds
strip StrictModeContext via DISABLE_STRICT_MODE_CONTEXT, Android WebView retains
StrictModeContext to respect host app policies, making unconditional
try-with-resources blocks a runtime performance penalty in release builds.

## Examples

Bad:

```cpp
// StrictModeContext executes unconditionally even in WebView release builds
// where R8 eliminates reflection and disk reads at build time.
try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
    return loadClass(clazz);
}
```

Good:

```cpp
// In release builds, R8 optimizes away disk reads/reflection at build time, bypassing StrictModeContext.
if (BuildConfig.IS_PROGUARD_ENABLED) {
    return loadClass(clazz);
}
try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
    return loadClass(clazz);
}
```
