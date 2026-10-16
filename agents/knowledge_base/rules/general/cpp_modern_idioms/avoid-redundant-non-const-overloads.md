---
name: general/avoid-redundant-non-const-overloads
rubric: readability
severity: nit
---

**Rule.** Do not provide a non-const overload for a lookup, query, or getter
method unless non-const callers genuinely require mutable access to the returned
object. Because non-const instances can invoke const member functions directly,
provide only the const member function when read-only access is sufficient.

**Why.** In C++, non-const instances can call const member functions directly.
Adding unnecessary non-const overloads increases maintenance boilerplate, risks
implementation drift, and unnecessarily exposes internal data structures to
accidental mutation.

## Examples

Bad:

```cpp
// Redundant non-const overload when callers only perform read-only lookups
ALWAYS_INLINE Node* FindNode(const Bucket& bucket, void* key);
ALWAYS_INLINE const Node* FindNode(const Bucket& bucket, void* key) const;
```

Good:

```cpp
// A single const method suffices since non-const instances can invoke const methods
ALWAYS_INLINE const Node* FindNode(const Bucket& bucket, void* key) const;
```
