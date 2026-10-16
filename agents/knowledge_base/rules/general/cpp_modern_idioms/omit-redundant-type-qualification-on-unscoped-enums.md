---
name: general/omit-redundant-type-qualification-on-unscoped-enums
rubric: readability
severity: nit
---

**Rule.** Do not qualify unscoped enum constants (such as C-style or IDL
enumerations) with their enum type name (e.g., prefer PROTECTION_NONE over
ProtectionLevel::PROTECTION_NONE) when the enumerators are exported directly
into the enclosing scope and use distinctive type-prefixed names.

**Why.** Unscoped enumerations export their constants directly into the
enclosing scope, and their macro-style names typically already incorporate the
type prefix to avoid collisions. Explicitly qualifying them with their enum type
name introduces unnecessary repetition and visual clutter without adding clarity
or safety.

## Examples

Bad:

```cpp
switch (level) {
  case ProtectionLevel::PROTECTION_NONE:
    return std::vector<uint8_t>{ProtectionLevel::PROTECTION_NONE};
  case ProtectionLevel::PROTECTION_PATH_VALIDATION:
    return GenerateValidationData(ProtectionLevel::PROTECTION_PATH_VALIDATION, process);
}
```

Good:

```cpp
switch (level) {
  case PROTECTION_NONE:
    return std::vector<uint8_t>{PROTECTION_NONE};
  case PROTECTION_PATH_VALIDATION:
    return GenerateValidationData(PROTECTION_PATH_VALIDATION, process);
}
```
