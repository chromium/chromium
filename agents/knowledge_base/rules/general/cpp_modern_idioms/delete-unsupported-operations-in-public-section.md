---
name: general/delete-unsupported-operations-in-public-section
rubric: readability
severity: should_fix
---

**Rule.** To forbid comparison operators or other unsupported operations on a
class, explicitly declare them with `= delete;` in the `public:` section of the
class definition. Do not declare unsupported operations without implementation
in a `private:` section.

**Why.** Declaring unimplemented functions in `private:` was a pre-C++11 idiom
that produces confusing access-violation compile errors rather than clear
deleted-function diagnostics. Placing `= delete;` in the `public:` section makes
the interface contract clear to readers, compiler diagnostics, and automated
tooling like C++20 operator rewrites.

## Examples

Bad:

```cpp
class ScopedPropVariant {
 private:
  // Comparison operators for ScopedPropVariant are not supported.
  bool operator==(const ScopedPropVariant&) const;
  bool operator!=(const ScopedPropVariant&) const;
};
```

Good:

```cpp
class ScopedPropVariant {
 public:
  // Comparison operators for ScopedPropVariant are not supported.
  bool operator==(const ScopedPropVariant&) const = delete;
  bool operator!=(const ScopedPropVariant&) const = delete;
};
```
