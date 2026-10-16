---
name: general/include-attribute-in-getters-returning-dimensions
rubric: readability
severity: nit
---

**Rule.** When a function or getter returns a scalar attribute or dimension of
an entity (such as its size, length, count, capacity, or offset) rather than the
entity itself, explicitly include the attribute in the function name (e.g.,
`Get<Entity>Size()`, `Get<Entity>Count()`, or `<entity>_size()`). Do not name
the function after the bare entity (e.g., `Get<Entity>()`), which implies
returning the entity itself (such as an object, memory range, or container).

**Why.** A function named `Get<Entity>()` implies returning the object,
descriptor, or resource itself. When the function returns a scalar metric (such
as a byte size, element count, or offset), omitting the attribute name misleads
callers about the return value's type and semantics.

## Examples

Bad:

```cpp
// Returns the byte size of the OS-based zero segment.
size_t GetZeroSegmentFromOS();

// Returns the number of registered observers.
size_t GetObservers();
```

Good:

```cpp
// Returns the byte size of the OS-based zero segment.
size_t GetZeroSegmentSizeFromOS();

// Returns the number of registered observers.
size_t GetObserverCount();
```
