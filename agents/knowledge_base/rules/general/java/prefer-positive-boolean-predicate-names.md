---
name: general/prefer-positive-boolean-predicate-names
rubric: readability
severity: optional
---

**Rule.** When defining boolean predicates, validation functions, or query
helpers (especially those intended for assertions or preconditions), name them
positively (such as IsInBounds(), IsValid(), or IsSupported()) rather than
negatively (such as IsOutOfBounds(), IsInvalid(), or IsNotSupported()). Avoid
forcing callers to negate negative predicates (e.g. CHECK(!IsOutOfBounds(...)))
to verify valid state.

**Why.** Asserting expected conditions directly (e.g. CHECK(IsInBounds(...))) is
clearer and more intuitive than double negatives or negating negative predicates
(CHECK(!IsOutOfBounds(...))). Positive predicate names reduce cognitive load,
prevent logical inversion mistakes in conditional expressions and assertions,
and align with idiomatic assertion styles.

## Examples

Bad:

```cpp
// Negative predicate forces callers to negate checks: CHECK(!IsExtentOutOfBounds(...))
template <typename T>
bool IsExtentOutOfBounds(const T* ptr,
                         internal::base::StrictNumeric<size_t> index);

// Negative predicate creates awkward double-negation in assertions.
bool IsInvalidHandler(const Handler* handler);
```

Good:

```cpp
// Positive predicate enables clear, direct assertions: CHECK(IsExtentInBounds(...))
template <typename T>
bool IsExtentInBounds(const T* ptr,
                      internal::base::StrictNumeric<size_t> index);

// Positive predicate expresses valid state directly: CHECK(IsValidHandler(handler))
bool IsValidHandler(const Handler* handler);
```
