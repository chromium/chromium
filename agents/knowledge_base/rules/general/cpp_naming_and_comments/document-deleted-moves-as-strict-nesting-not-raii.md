---
name: general/document-deleted-moves-as-strict-nesting-not-raii
rubric: readability
severity: nit
---

**Rule.** When documenting why move constructors and move assignment operators
are deleted on a scoped object or RAII guard, do not claim that deleting moves
enforces RAII. Accurately state that disabling move operations prevents scopes
from interleaving and ensures they cleanly nest in lexical LIFO order.

**Why.** Move semantics preserve RAII because transferred resources are still
released deterministically upon destruction of the destination object. Deleting
move operations on scoped guards enforces the strictly stronger guarantee that
lifetimes cannot escape their declaring lexical block or overlap out-of-order,
preventing interleaved scopes.

## Examples

Bad:

```cpp
// Disable move constructor and move assignment operator to enforce RAII
// usage.
ScopedServiceBindingBatch(ScopedServiceBindingBatch&&) = delete;
ScopedServiceBindingBatch& operator=(ScopedServiceBindingBatch&&) = delete;
```

Good:

```cpp
// Disable move constructor and move assignment operator to ensure that
// scopes are not interleaved, but cleanly nested.
ScopedServiceBindingBatch(ScopedServiceBindingBatch&&) = delete;
ScopedServiceBindingBatch& operator=(ScopedServiceBindingBatch&&) = delete;
```
