---
name: general/explain-why-not-what-in-inline-comments
rubric: readability
severity: nit
---

**Rule.** Do not write inline comments that merely restate or paraphrase what
the adjacent code literally does (such as repeating a condition or routine
assignment). Instead, write comments that explain the 'why'—the underlying
motivation, non-obvious constraints, architectural rationale, or why a specific
mechanism was chosen.

**Why.** Readers can already see what the code does by reading the syntax.
Restating the implementation mechanics in comments creates redundant noise
without aiding comprehension, whereas explaining the underlying design
decisions, trade-offs, and external constraints provides context that the code
itself cannot convey.

## Examples

Bad:

```cpp
# Add `-fsanitize-recover=address` only if use_asan_backup_ref_ptr_v2=true.
if (use_asan_backup_ref_ptr_v2) {
  all_dependent_configs += [ ":asan_raw_ptr_service_v2_config" ]
}
```

Good:

```cpp
# Moves the `-fsanitize-recover=address` configuration to `//base` using
# `all_dependent_configs`. This ensures the flag is only active when
# `use_asan_backup_ref_ptr_v2=true`, preventing PartitionAlloc GN variables
# from polluting the global `//build` directory and breaking standalone
# projects like V8.
if (use_asan_backup_ref_ptr_v2) {
  all_dependent_configs += [ ":asan_raw_ptr_service_v2_config" ]
}
```
