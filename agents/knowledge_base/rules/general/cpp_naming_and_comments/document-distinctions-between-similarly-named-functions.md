---
name: general/document-distinctions-between-similarly-named-functions
rubric: readability
severity: nit
---

**Rule.** When declaring or defining multiple functions, methods, API variants,
or wrapper overloads with similar names or related contracts, document their
concrete distinctions, preconditions, and intended use cases rather than using
vague phrases like 'with different semantics'. When introducing a wrapper,
overload, or convenience function for an API with documented caveats or hazards,
explicitly state in its doc comment that the same caveats apply (or reference
the underlying function's caveats).

**Why.** Vague comparative comments or missing caveat notices on wrapper
functions leave callers uncertain of memory safety constraints, preconditions,
and return contracts. Explicitly documenting concrete distinctions and inherited
caveats prevents API misuse and subtle correctness bugs.

## Examples

Bad:

```cpp
// Variant 1: Undocumented distinction between sibling methods.
static bool IsValidPtr(ThreadCache* tcache) {
  return reinterpret_cast<uintptr_t>(tcache) & kTombstoneMask;
}
static bool IsValid(ThreadCache* tcache) {
  return tcache && tcache->root_;
}

// Variant 2: Vague comparative comment on an API variant.
// Similar to the above, but pool-agnostic and with different semantics.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
bool IsExtentOutOfBounds(uintptr_t ptr,
                         size_t extent_bytes,
                         size_t type_size);

// Variant 3: Wrapper omitting inherited caveats from underlying API.
// Suitable for external callers.
template <typename T>
bool IsExtentInBounds(const T* ptr,
                      internal::base::StrictNumeric<size_t> index);
```

Good:

```cpp
// Variant 1: Concrete distinction documented for each sibling method.
// Returns true if the pointer is non-null and not a tombstone sentinel.
static bool IsValidPtr(ThreadCache* tcache) {
  return reinterpret_cast<uintptr_t>(tcache) & kTombstoneMask;
}
// Returns true if the thread cache pointer is valid and fully initialized.
static bool IsValid(ThreadCache* tcache) {
  return IsValidPtr(tcache) && tcache->root_;
}

// Variant 2: Specific variant differences articulated directly.
// Pool-agnostic version of the above. Primarily used to support Checked
// Span (https://crbug.com/484171909).
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
bool IsExtentOutOfBounds(uintptr_t ptr,
                         size_t extent_bytes,
                         size_t type_size);

// Variant 3: Wrapper explicitly references inherited caveats.
// Suitable for external callers. Has the same caveats as
// `IsExtentOutOfBounds()` (but inverted).
template <typename T>
bool IsExtentInBounds(const T* ptr,
                      internal::base::StrictNumeric<size_t> index);
```
