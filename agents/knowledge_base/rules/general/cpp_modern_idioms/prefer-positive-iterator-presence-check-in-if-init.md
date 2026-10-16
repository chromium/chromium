---
name: general/prefer-positive-iterator-presence-check-in-if-init
rubric: readability
severity: nit
---

**Rule.** When initializing an iterator or lookup result in an `if`
init-statement (e.g., `if (auto it = container.find(key); ...)`), test that the
element was found (`it != container.end()`) so the branch consuming `it`
immediately follows its declaration. Do not test `it == container.end()` and
defer using `it` to an `else` branch.

**Why.** Testing `it != container.end()` keeps the block consuming the
initialized variable directly adjacent to where it is declared, improving code
locality and readability.

## Examples

Bad:

```cpp
if (auto it = map.find(key); it == map.end()) {
  HandleNotFound();
} else {
  Use(it->second);
}
```

Good:

```cpp
if (auto it = map.find(key); it != map.end()) {
  Use(it->second);
} else {
  HandleNotFound();
}
```
