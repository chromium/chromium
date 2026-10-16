---
name: general/suffix-eager-vec-allocating-rust-methods-with-to-vec
rubric: readability
severity: should_fix
---

**Rule.** In Rust APIs, append `_to_vec` to method names that eagerly allocate
and return a `Vec` (e.g. `fallback_to_vec`) rather than lazily yielding an
iterator. Reserve unadorned operation names for zero-allocation accessors,
borrowed views, or lazy iterator interfaces.

**Why.** Eagerly allocating and returning a `Vec` imposes heap allocation
overhead that is not obvious to callers from a generic operation name. Adding
`_to_vec` makes heap allocation explicit at call sites and preserves clean,
unadorned identifiers for future zero-allocation or lazy iterator methods.

## Examples

Bad:

```cpp
pub fn fallback(&self, locale_bytes: &[u8]) -> Vec<Icu4xLocale> {
    let mut results = Vec::new();
    // Eagerly populates and returns a heap-allocated Vec.
    results
}
```

Good:

```cpp
pub fn fallback_to_vec(&self, locale_bytes: &[u8]) -> Vec<Icu4xLocale> {
    let mut results = Vec::new();
    // Explicit suffix alerts callers to the heap allocation.
    results
}
```
