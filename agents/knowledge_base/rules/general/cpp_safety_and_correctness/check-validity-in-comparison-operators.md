---
name: general/check-validity-in-comparison-operators
rubric: correctness
severity: should_fix
---

**Rule.** When implementing comparison operators (operator==, operator\<=>) or
hash functions/functors (operator(), std::hash<T>, AbslHashValue) for a value
type that tracks validity (e.g. via is_valid() or an internal status),
explicitly include validity checks if the invalid state's backing storage can
coincide with a valid value (such as all-zero bytes for both invalid and nil
instances). In comparison operators, ensure invalid objects do not compare equal
to valid ones; in hash functions, return a distinct sentinel hash value (such as
0\) for invalid instances rather than hashing raw bytes directly.

**Why.** If an invalid object's underlying storage matches a valid
representation (such as all-zero bytes representing a valid nil object),
comparing or hashing raw storage causes invalid objects to erroneously compare
equal to valid objects, sort identically, or collide in hash containers.
Explicitly checking validity preserves separation between valid and invalid
instances across equality, ordering, and hashing.

## Examples

Bad:

```cpp
// Bad: Invalid instances with all-zero storage compare equal to and collide with valid Nil objects.
friend bool operator==(const Uuid& lhs, const Uuid& rhs) {
  return lhs.uuid_bytes_ == rhs.uuid_bytes_;
}

friend std::strong_ordering operator<=>(const Uuid& lhs, const Uuid& rhs) {
  return lhs.uuid_bytes_ <=> rhs.uuid_bytes_;
}

size_t UuidHash::operator()(const Uuid& uuid) const {
  return FastHash(uuid.GetBytes());
}
```

Good:

```cpp
// Good: Explicitly check validity in comparison operators and hash functions.
friend bool operator==(const Uuid& lhs, const Uuid& rhs) {
  return lhs.is_valid() == rhs.is_valid() &&
         lhs.uuid_bytes_ == rhs.uuid_bytes_;
}

friend std::strong_ordering operator<=>(const Uuid& lhs, const Uuid& rhs) {
  if (lhs.is_valid() != rhs.is_valid()) {
    return lhs.is_valid() ? std::strong_ordering::greater
                          : std::strong_ordering::less;
  }
  return lhs.uuid_bytes_ <=> rhs.uuid_bytes_;
}

size_t UuidHash::operator()(const Uuid& uuid) const {
  if (!uuid.is_valid()) {
    return 0;
  }
  return FastHash(uuid.GetBytes());
}
```
