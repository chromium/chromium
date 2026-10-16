---
name: general/omit-subsumed-flags-in-gn-conditions
rubric: readability
severity: nit
---

**Rule.** In GN build files (`.gn`, `.gni`), do not include redundant operands
in boolean conditions when one flag or condition strictly implies the other
(e.g., `if (variant_flag || base_flag)` when `variant_flag` requires
`base_flag`). Guard shared logic using only the base flag, and nest or isolate
variant-specific additions under the sub-feature flag.

**Why.** When a sub-feature or variant flag requires its base feature flag to be
active (via build assertions or derivation), testing `variant || base` is
tautological because `base` already subsumes `variant`. Omitting the redundant
operand simplifies build logic and avoids confusing readers about flag
dependencies.

## Examples

Bad:

```cpp
# Bad: use_asan_backup_ref_ptr_v2 strictly requires use_asan_backup_ref_ptr.
if (use_asan_backup_ref_ptr_v2 || use_asan_backup_ref_ptr) {
  sources += [ "memory/raw_ptr_asan_allocator.cc" ]
  if (use_asan_backup_ref_ptr_v2) {
    deps += [ "//build/config/sanitizers:sanitizer_shared_hooks" ]
  }
}
```

Good:

```cpp
# Good: Guard shared logic with the base flag alone.
if (use_asan_backup_ref_ptr) {
  sources += [ "memory/raw_ptr_asan_allocator.cc" ]
  if (use_asan_backup_ref_ptr_v2) {
    deps += [ "//build/config/sanitizers:sanitizer_shared_hooks" ]
  }
}
```
