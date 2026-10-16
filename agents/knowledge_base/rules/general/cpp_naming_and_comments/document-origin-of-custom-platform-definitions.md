---
name: general/document-origin-of-custom-platform-definitions
rubric: readability
severity: optional
---

**Rule.** When defining custom, undeclared, or internal platform structures,
types, or macros (such as Windows NT-level definitions, undeclared ioctl
constants, or internal OS kernel shims omitted from public SDK headers),
document their origin and reference in comments explaining where they derive
from and why custom definitions are necessary. Where feasible, use
static_asserts to verify that custom structures match equivalent public SDK or
platform definitions.

**Why.** Custom or internal platform definitions lack standard SDK header
documentation. Adding comments citing their origin headers or specifications and
explaining why public definitions could not be used makes the codebase auditable
and prevents breakage during OS or SDK upgrades.

## Examples

Bad:

```cpp
// Define security attribute definitions.
typedef struct _TOKEN_SECURITY_ATTRIBUTE_V1 {
  UNICODE_STRING Name;
  USHORT ValueType;
  USHORT Reserved;
  ULONG Flags;
  ULONG ValueCount;
  union {
    PLONG64 pInt64;
    PULONG64 pUint64;
    PUNICODE_STRING pString;
  } Values;
} TOKEN_SECURITY_ATTRIBUTE_V1, *PTOKEN_SECURITY_ATTRIBUTE_V1;
```

Good:

```cpp
// These are the NT versions of the AUTHZ_SECURITY_ATTRIBUTE* defines in authz.h
// and CLAIM_SECURITY_ATTRIBUTE* in winnt.h. We must use the TOKEN_ versions
// here as they use UNICODE_STRING instead of PWSTR. Where possible,
// static_asserts verify that the structures match the definitions from other
// header files and will fail if they ever change.
typedef struct _TOKEN_SECURITY_ATTRIBUTE_V1 {
  UNICODE_STRING Name;
  USHORT ValueType;
  USHORT Reserved;
  ULONG Flags;
  ULONG ValueCount;
  union {
    PLONG64 pInt64;
    PULONG64 pUint64;
    PUNICODE_STRING pString;
  } Values;
} TOKEN_SECURITY_ATTRIBUTE_V1, *PTOKEN_SECURITY_ATTRIBUTE_V1;
```
