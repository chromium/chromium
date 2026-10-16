---
name: general/document-mutually-exclusive-returns-disjunctively
rubric: readability
severity: nit
---

**Rule.** When documenting functions or methods that return multi-state or
tri-state outcomes (such as `base::expected<bool, E>`, `std::optional<T>`, enum
states, or success/failure codes), enumerate the possible return values
disjunctively using 'or' rather than 'and' (e.g., 'Returns `true` if ...,
`false` if ..., or an error otherwise').

**Why.** Using conjunctive phrasing ('and an error') when describing alternative
return states implies that multiple outcomes occur simultaneously or confuses
the return contract. Disjunctive phrasing ('or') explicitly communicates that
the function yields exactly one of several mutually exclusive outcomes.

## Examples

Bad:

```cpp
// Returns `true` if the attempt succeeded, `false` if the module is not
// a delayloaded dep of the current module, and an error with the relevant
// HRESULT otherwise.
base::expected<bool, HRESULT> LoadAllImportsForDll(const char* dll_name);
```

Good:

```cpp
// Returns `true` if the attempt succeeded, `false` if the module is not
// a delayloaded dep of the current module, or an `HRESULT` error
// otherwise.
base::expected<bool, HRESULT> LoadAllImportsForDll(const char* dll_name);
```
