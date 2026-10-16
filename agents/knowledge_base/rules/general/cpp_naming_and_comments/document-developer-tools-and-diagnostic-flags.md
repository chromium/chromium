---
name: general/document-developer-tools-and-diagnostic-flags
rubric: readability
severity: optional
---

**Rule.** When introducing developer-facing tools, diagnostic build flags, or
instrumentation options, provide accompanying documentation (in Markdown docs or
guides) explaining how to use the tooling. Include required GN build arguments
(such as non-debug builds or warning flags), sample diagnostic output, and
actionable guidance on how to interpret and resolve the findings.

**Why.** Developer-facing diagnostic tools and build flags often require
specific build configurations and produce complex output that is difficult to
act on without context. Providing documented GN arguments, sample output, and
remediation patterns ensures developers can discover, configure, and effectively
resolve issues identified by the tooling.

## Examples

Bad:

```cpp
// In base/debug/debug.gni:
declare_args() {
  # Anti-pattern: diagnostic flag added without developer documentation on
  # required GN arguments, expected diagnostic output, or remediation steps.
  enable_check_elision_warning = false
}
```

Good:

````cpp
### Identifying span bounds-checking

The bounds-checking that is built into span operations is often elided
by the compiler at build time. To identify remaining checks:

```gn
is_debug = false  # Must run an optimized build
treat_warnings_as_errors = false  # Warnings will be emitted
enable_check_elision_warning = true
````

Such a build produces diagnostic warnings. Example output and remediation:

```
warning: call to 'base::check_not_elided()' declared with 'warning' attribute
```

Fix by proving bounds to the compiler or restructuring the loop.

```
```
