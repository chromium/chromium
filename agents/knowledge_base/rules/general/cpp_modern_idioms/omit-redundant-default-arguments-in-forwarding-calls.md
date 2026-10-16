---
name: general/omit-redundant-default-arguments-in-forwarding-calls
rubric: readability
severity: nit
---

**Rule.** Do not explicitly pass arguments that duplicate a callee's default
parameter values in macro wrappers, forwarding helpers, or delegating call
sites. Rely on the declared default parameter value to keep call sites clean and
prevent divergence if the default changes.

**Why.** Passing arguments that duplicate a callee's default parameter values
adds unnecessary boilerplate and creates inconsistency across sibling macro
variants or forwarding callers. If the default parameter value is updated in the
callee declaration, explicitly hardcoded duplicates will fail to adopt the new
default.

## Examples

Bad:

```cpp
// Feature constructor defaults early access to FEATURE_EARLY_ACCESS_NOT_ALLOWED.
#define BASE_FEATURE_INTERNAL_2_ARGS(feature, default_state)              \
  constinit const base::Feature feature(                                  \
      []() {                                                              \
        static_assert(#feature[0] == 'k');                                \
        static constexpr base::internal::StringStorage storage(#feature); \
        return storage.storage.data();                                    \
      }(),                                                                \
      default_state, base::internal::FeatureMacroHandshake::kSecret,      \
      base::FEATURE_EARLY_ACCESS_NOT_ALLOWED)
```

Good:

```cpp
// Feature constructor defaults early access to FEATURE_EARLY_ACCESS_NOT_ALLOWED.
#define BASE_FEATURE_INTERNAL_2_ARGS(feature, default_state)              \
  constinit const base::Feature feature(                                  \
      []() {                                                              \
        static_assert(#feature[0] == 'k');                                \
        static constexpr base::internal::StringStorage storage(#feature); \
        return storage.storage.data();                                    \
      }(),                                                                \
      default_state, base::internal::FeatureMacroHandshake::kSecret)
```
