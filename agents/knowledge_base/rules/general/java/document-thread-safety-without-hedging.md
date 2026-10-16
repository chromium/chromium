---
name: general/document-thread-safety-without-hedging
rubric: readability
severity: should_fix
---

**Rule.** Do not use vague hedging qualifiers such as 'generally', 'mostly',
'usually', or 'typically' when documenting thread-safety or concurrency
contracts on classes, methods, or variables. State whether an API is
unconditionally thread-safe, or explicitly document concrete threading
constraints, exceptions, and ordering requirements. When documenting
initialization, startup, or configuration functions that mutate global or static
state, explicitly state any main-thread constraints alongside lifecycle and
timing requirements (such as being called only once after feature list
initialization). Accurately document if functions remain thread-unsafe due to
unsynchronized global state or test overrides rather than omitting thread-safety
warnings.

**Why.** Vague qualifiers like 'generally thread-safe' leave callers uncertain
about when concurrent access is safe. Similarly, omitting thread-safety
restrictions on wrapper utilities or global initialization functions misleads
callers into invoking them from worker threads, causing data races on
unsynchronized process-global state.

## Examples

Bad:

```cpp
// Bad: Hedging qualifier in doc comment
/** ID generation and access is generally thread-safe. */
public class IdSequencer { ... }

// Bad: Removing thread-safety warning because NSBundle is thread-safe
namespace base::apple {
BASE_EXPORT NSBundle* MainBundle();
BASE_EXPORT void SetOverrideFrameworkBundle(NSBundle* bundle);
}

// Bad: Missing main-thread constraint on global feature initialization
// Applies features configured by field trials to base::Lock. Must be called
// only after the feature list has been initialized and only once.
static void InitializeFeatures();
```

Good:

```cpp
// Good: Authoritative thread-safety contract
/** ID generation and access is thread-safe. */
public class IdSequencer { ... }

// Good: Accurately documenting thread-safety caveats from global overrides
// The functions in this file are not thread-safe. While NSBundle itself is
// thread-safe, the override functions in this file mutate global state without
// synchronization. Therefore, all functions in this file should be called on
// the main thread.
namespace base::apple {
BASE_EXPORT NSBundle* MainBundle();
BASE_EXPORT void SetOverrideFrameworkBundle(NSBundle* bundle);
}

// Good: Explicitly documenting main-thread requirement alongside timing
// Applies features configured by field trials to base::Lock. Must be called
// on the main thread only after the feature list has been initialized and
// only once.
static void InitializeFeatures();
```
