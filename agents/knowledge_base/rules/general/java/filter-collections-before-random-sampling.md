---
name: general/filter-collections-before-random-sampling
rubric: readability
severity: nit
---

**Rule.** In Swift, when selecting a random element from a collection or set
while excluding specific items, filter or subtract the excluded items upfront
(e.g. via Set.subtracting(_), Set.intersection(_), or Collection.filter(\_)) and
pick with .randomElement(). Do not repeatedly sample the collection in a bounded
retry loop to find a non-matching element.

**Why.** Sampling in a bounded retry loop to skip unwanted elements is
non-deterministic and can fail prematurely even when valid elements exist.
Declaratively filtering or subtracting excluded items beforehand guarantees
picking an eligible element in a single step whenever the filtered collection is
non-empty.

## Examples

Bad:

```cpp
// Bad: Sampling in a retry loop is non-deterministic and verbose.
var eventName: String = "$"
for _ in 0..<3 {
  eventName = ILType.jsLockRequestCallbackRouter.randomProperty()!
  if eventName != "$" {
    break
  }
}
guard eventName != "$" else { return }
```

Good:

```cpp
// Good: Subtract excluded elements upfront and sample with randomElement().
guard
  let eventName = ILType.jsLockRequestCallbackRouter.properties
    .subtracting(["$"])
    .randomElement()
else {
  return
}
```
