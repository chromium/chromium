---
name: general/avoid-component-imports-in-build-configs
rubric: design
severity: should_fix
---

**Rule.** Do not import component-specific `.gni` files or evaluate
component-level build flags inside `//build` configurations; `//build` must
remain independent of higher layers. Keep shared toolchain and compiler configs
in `//build` general and gate their application (via `configs`,
`public_configs`, or `all_dependent_configs`) conditionally inside the consumer
target.

**Why.** `//build` is a foundational configuration layer shared across projects
(such as Chromium, V8, and WebRTC). Importing component-level `.gni` files or
referencing higher-layer build flags inside `//build` introduces circular
dependencies, leaks Chromium-specific configurations into external projects, and
breaks build layering.

## Examples

Bad:

```cpp
# In build/config/sanitizers/BUILD.gn:
import("//base/allocator/partition_allocator/partition_alloc.gni")

config("alloc_token") {
  if (is_clang && shim_supports_alloc_token) {
    cflags = [ "-fsanitize=alloc-token" ]
  }
}

# In base/allocator/partition_allocator/src/partition_alloc/BUILD.gn:
all_dependent_configs += [ "//build/config/sanitizers:alloc_token" ]
```

Good:

```cpp
# In build/config/sanitizers/BUILD.gn:
config("alloc_token") {
  if (is_clang) {
    cflags = [ "-fsanitize=alloc-token" ]
  }
}

# In base/allocator/partition_allocator/src/partition_alloc/BUILD.gn:
if (shim_supports_alloc_token) {
  all_dependent_configs += [ "//build/config/sanitizers:alloc_token" ]
}
```
