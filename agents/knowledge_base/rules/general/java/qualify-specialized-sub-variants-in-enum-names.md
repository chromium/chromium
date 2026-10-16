---
name: general/qualify-specialized-sub-variants-in-enum-names
rubric: readability
severity: nit
---

**Rule.** When defining enum constants, process/thread types, or categories that
identify a specific sub-category or specialized variant of a broader feature
(such as Top Chrome WebUI vs. general WebUI), qualify the name with the specific
sub-variant rather than using the bare generic category name. Preserve the
unqualified category name for the general or regular case.

**Why.** Using a broad category identifier for a specialized sub-variant
mischaracterizes the scope of what is being tagged or measured in telemetry and
prevents cleanly introducing or distinguishing the broader or regular case in
the future.

## Examples

Bad:

```cpp
// Misleadingly uses the generic category name for a specialized sub-variant:
enum ProcessType {
  PROCESS_UNSPECIFIED = 0;
  PROCESS_RENDERER_WEBUI = 1;
};

// In C++:
enum class ProcessType {
  kUnspecified = 0,
  kRendererWebUI = 1,
};
```

Good:

```cpp
// Accurately includes the qualifying prefix for the specialized sub-variant:
enum ProcessType {
  PROCESS_UNSPECIFIED = 0;
  PROCESS_RENDERER_TOP_WEBUI = 1;
};

// In C++:
enum class ProcessType {
  kUnspecified = 0,
  kRendererTopWebUI = 1,
};
```
