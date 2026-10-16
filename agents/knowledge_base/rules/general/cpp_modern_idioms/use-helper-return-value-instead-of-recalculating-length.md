---
name: general/use-helper-return-value-instead-of-recalculating-length
rubric: readability
severity: nit
---

**Rule.** When a helper function returns a computed length, size, or offset
(such as parsed string length including delimiters or terminators), consume the
returned value at call sites instead of discarding it and recalculating the size
manually (e.g., `str.size() + 1`). If the computed return value is genuinely
never needed by any caller, change the helper's return type to `void`.

**Why.** Discarding a helper's return value to recompute the same value via
manual arithmetic at call sites introduces redundant operations and risks
inconsistencies or off-by-one errors if delimiter or terminator handling
changes. It also leaves misleading unused return types in helper signatures.

## Examples

Bad:

```cpp
// ParseEnvLine returns the total line length including the null terminator.
ParseEnvLine(env_var, &key);
if (changes.find(key) == changes.end()) {
  result.append(env_var.c_str(), env_var.size() + 1);
}
```

Good:

```cpp
// Consume the helper's returned length instead of recalculating manually.
size_t line_length = ParseEnvLine(env_var, &key);
if (changes.find(key) == changes.end()) {
  result.append(env_var.c_str(), line_length);
}
```
