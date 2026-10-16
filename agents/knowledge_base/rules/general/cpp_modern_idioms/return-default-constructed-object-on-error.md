---
name: general/return-default-constructed-object-on-error
rubric: readability
severity: nit
---

**Rule.** In parser, deserializer, or factory functions that return a default or
invalid instance on error, return a fresh default-constructed object (such as
`return Type();` or `return {};`) directly. Do not manually clear, zero, or
reset fields on an in-progress local object before returning it.

**Why.** Returning a fresh default-constructed object directly ensures the
returned value is in a canonical default or invalid state, prevents unreset or
partially populated fields as the class evolves, and eliminates unnecessary
cleanup boilerplate on failure paths.

## Examples

Bad:

```cpp
// Bad: Manually clearing fields on an in-progress local object before returning on error.
Uuid Uuid::ParseLowercase(std::string_view input) {
  Uuid uuid;
  // ... parse bytes ...
  if (!IsLowerHexDigit(input[i])) {
    uuid.uuid_bytes_ = {0};
    return uuid;
  }
  return uuid;
}
```

Good:

```cpp
// Good: Return a fresh default-constructed instance directly on parse failure.
Uuid Uuid::ParseLowercase(std::string_view input) {
  Uuid uuid;
  // ... parse bytes ...
  if (!IsLowerHexDigit(input[i])) {
    return Uuid();
  }
  return uuid;
}
```
