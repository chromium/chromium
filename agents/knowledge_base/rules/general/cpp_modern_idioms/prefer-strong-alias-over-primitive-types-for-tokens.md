---
name: general/prefer-strong-alias-over-primitive-types-for-tokens
rubric: readability
severity: nit
---

**Rule.** Prefer strongly typed wrappers (such as base::StrongAlias,
base::IdType, or base::TokenType) over bare primitive types (e.g., size_t, int,
bool) or weak using/typedef aliases for domain-specific tokens, identifiers, and
parameters, particularly when passed alongside other arguments of the same
underlying type.

**Why.** Raw primitive types and weak type aliases do not prevent accidental
argument transposition at call sites when multiple parameters share the same
underlying representation. Strongly typed wrappers enforce type safety at
compile time without runtime overhead.

## Examples

Bad:

```cpp
// Weak alias or bare primitive allows accidental argument swapping with size
using AllocToken = size_t;
void* Alloc(size_t size, AllocToken token, void* context);
```

Good:

```cpp
// StrongAlias enforces compile-time type separation
using AllocToken = base::StrongAlias<class AllocTokenTag, size_t>;
void* Alloc(size_t size, AllocToken token, void* context);
```
