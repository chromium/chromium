---
name: general/document-gn-declare-args-build-arguments
rubric: readability
severity: should_fix
---

**Rule.** Precede every build argument declared within `declare_args()` blocks
in `.gn` and `.gni` files with a descriptive comment explaining what feature,
behavior, or dependency the flag controls. Write comments as complete sentences,
as GN tooling (such as `gn args --list`) extracts comments immediately preceding
each argument to generate public documentation.

**Why.** Variables declared in `declare_args()` blocks define public build
arguments configurable by developers and build configurations. GN tooling
(`gn args --list`) automatically parses preceding comments as documentation;
omitting comments leaves users without context on what the flag controls, its
default behavior, or when it should be set.

## Examples

Bad:

```cpp
declare_args() {
  # Anti-pattern: undocumented build arguments in declare_args block
  shim_supports_alloc_token = false
  enable_custom_allocator_tracking = true
}
```

Good:

```cpp
declare_args() {
  # Whether the allocator shim supports alloc token or not. If true, malloc()
  # and operator new() families are injected with an alloc token parameter,
  # which is used to select the partition in PartitionAlloc.
  shim_supports_alloc_token = false

  # Enables custom allocator allocation tracking for performance analysis.
  enable_custom_allocator_tracking = true
}
```
