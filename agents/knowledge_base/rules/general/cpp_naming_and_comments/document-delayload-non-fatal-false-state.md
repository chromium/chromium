---
name: general/document-delayload-non-fatal-false-state
rubric: readability
severity: optional
---

**Rule.** When calling base::win::LoadAllImportsForDllUnchecked() (or
LoadAllImportsForDll()), add an explanatory comment if treating
loaded.has_value() as success rather than loaded.value_or(false). Clarify in the
comment that a false value indicates the library was not present in the
delay-import table (such as in tests or component builds) rather than an import
resolution failure.

**Why.** base::win::LoadAllImportsForDllUnchecked() returns
base::expected\<bool, HRESULT> where ok(true) means delayloads resolved,
ok(false) means the module is not in the delay-import table, and
unexpected(HRESULT) indicates resolution failed. Without documentation, readers
and reviewers naturally assume has_value() was an accidental bug for
value_or(false).

## Examples

Bad:

```cpp
// If the module is loaded, force resolve delayloads.
auto loaded = base::win::LoadAllImportsForDllUnchecked("dbghelp.dll");
return loaded.has_value();
```

Good:

```cpp
// If the module is loaded, force resolve delayloads.
auto loaded = base::win::LoadAllImportsForDllUnchecked("dbghelp.dll");
// In tests or component builds where dbghelp is not delayloaded, this
// returns ok(false); a failure only occurs when an error code is returned.
return loaded.has_value();
```
