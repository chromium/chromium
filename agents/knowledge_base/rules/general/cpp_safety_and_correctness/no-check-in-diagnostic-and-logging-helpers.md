---
name: general/no-check-in-diagnostic-and-logging-helpers
rubric: correctness
severity: should_fix
---

**Rule.** Do not use CHECK(), DCHECK(), or NOTREACHED() inside logging,
error-reporting, or diagnostic helpers (such as enum-to-string conversion
functions or crash key formatters). Return a safe fallback sentinel string (e.g.
"<unknown>" or "<unknown value>") or default value when encountering unmapped or
unexpected inputs.

**Why.** Diagnostic and logging helpers are frequently called during error
handling, tracing, and crash reporting where values may be uninitialized,
unmapped, or received from server configs. Crashing inside diagnostic helpers
turns benign or recoverable errors into fatal production crashes.

## Examples

Bad:

```cpp
template <typename Enum>
std::string GetEnumName(base::span<const Option<Enum>> options, Enum value) {
  for (const auto& option : options) {
    if (value == option.value) {
      return option.name;
    }
  }
  CHECK(false) << "Unknown enum value " << static_cast<int>(value);
  return "";
}
```

Good:

```cpp
template <typename Enum>
std::string GetEnumName(base::span<const Option<Enum>> options, Enum value) {
  for (const auto& option : options) {
    if (value == option.value) {
      return option.name;
    }
  }
  return "<unknown value>";
}
```
