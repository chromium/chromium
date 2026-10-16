---
name: general/keep-internal-rust-struct-fields-private
rubric: readability
severity: should_fix
---

**Rule.** When defining Rust structs, keep internal implementation details,
cached state, and private mechanics private (omitting pub) rather than exposing
them as public struct fields. Expose only fields intended for direct external
consumer access, or provide public accessor methods when controlled access is
required.

**Why.** Marking internal fields as pub breaks encapsulation and allows external
code to read or mutate internal state directly. Keeping internal fields private
preserves API boundaries, prevents accidental invalidation of struct invariants,
and avoids breaking callers during internal refactoring.

## Examples

Bad:

```cpp
pub struct Feature {
    pub name: &'static c_char,
    pub default_state: FeatureState,
    pub cached_value: AtomicU32,
}
```

Good:

```cpp
pub struct Feature {
    pub name: &'static c_char,
    pub default_state: FeatureState,
    cached_value: AtomicU32,
}
```
