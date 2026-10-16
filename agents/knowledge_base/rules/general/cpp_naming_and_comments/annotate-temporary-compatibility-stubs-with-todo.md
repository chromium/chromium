---
name: general/annotate-temporary-compatibility-stubs-with-todo
rubric: readability
severity: should_fix
---

**Rule.** When introducing temporary workarounds, fallback branches, intentional
exclusions, stub implementations, placeholder delegates, transitional template
parameters or default arguments (such as relaxed pointer safety traits), interim
migration restrictions, or API incompatibility constraints, explain the
underlying technical rationale and attach a tracking bug or bug-linked TODO
(e.g., `// TODO(crbug.com/<id>): ...` or `-- TODO(b/<id>): ...`) documenting the
migration path or removal condition. When retaining deprecated functions,
overloads, schema columns, or compatibility stubs alongside replacements,
document the migration path with a bug-linked TODO; do not remove a migration
TODO while leaving obsolete code in the repository.

**Why.** Temporary workarounds, transitional parameters, stub implementations,
intentional exclusions, interim restrictions, and deprecated APIs risk becoming
untracked technical debt or permanent escape hatches if left undocumented.
Explaining technical interactions clarifies design intent, while bug-linked
TODOs and issue references ensure temporary logic, transitional defaults, and
exclusions are tracked and cleaned up or migrated promptly.

## Examples

Bad:

```cpp
// Transitional template default parameter without removal criteria or tracking TODO:
template <class Source,
          class Observer,
          RawPtrTraits source_ptr_trait = LeakedDanglingUntriaged>
class ScopedObservation {

// Intentional exclusion/limitation left unexplained without a tracking bug:
_use_fuzztest_llvm_wrapper =
    use_centipede && !_exclude_main && !defined(invoker.seed_corpus)

// Placeholder delegate waiting for future platform release without a tracking TODO:
default boolean requestTextCursorOnScreen(View view, Rect boundsInView) {
  return false;
}

// Deprecated overload retained with TODO removed while obsolete function remains:
// Deprecated. Use span version instead.
UNSAFE_BUFFER_USAGE BASE_EXPORT bool ReadUnicodeCharacter(
    const char16_t* src, size_t src_len, size_t* char_index, base_icu::UChar32* code_point);
```

Good:

```cpp
// Transitional parameter documented with target default and bug-linked removal plan:
// TODO(crbug.com/493572976): The source_ptr_trait default will be changed to
// RawPtrTraits::kEmpty when all dangling instances are triaged, and removed once fixed.
template <class Source,
          class Observer,
          RawPtrTraits source_ptr_trait = LeakedDanglingUntriaged>
class ScopedObservation {

// Explanatory comment detailing the limitation and referencing tracking bug:
# Don't apply wrapper to targets with a seed corpus; they are unsupported
# and will be manually migrated to FUZZ_TEST. See crbug.com/505034799.
_use_fuzztest_llvm_wrapper =
    use_centipede && !_exclude_main && !defined(invoker.seed_corpus)

// Placeholder delegate annotated with tracking bug and target release:
default boolean requestTextCursorOnScreen(View view, Rect boundsInView) {
  // TODO(crbug.com/450540343): Inline internal delegate when API 36.1 releases.
  return false;
}

// Retain bug-linked TODO if deprecated overload must temporarily remain:
// TODO(crbug.com/40284755): Migrate callers to span version and delete.
UNSAFE_BUFFER_USAGE BASE_EXPORT bool ReadUnicodeCharacter(
    const char16_t* src, size_t src_len, size_t* char_index, base_icu::UChar32* code_point);
```
