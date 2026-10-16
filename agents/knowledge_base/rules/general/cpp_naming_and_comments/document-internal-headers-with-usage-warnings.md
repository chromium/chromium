---
name: general/document-internal-headers-with-usage-warnings
rubric: readability
severity: nit
---

**Rule.** In internal shim or implementation headers not intended for general
use, include a top-level file comment explicitly warning callers not to include
or use the header directly and directing them to the intended public API.

**Why.** Internal shims and decoupling headers can easily be accidentally
included or called across module boundaries. Adding an explicit warning comment
and directing callers to public headers prevents unintended dependencies and
architectural misuse.

## Examples

Bad:

```cpp
// Shim so the //base GN target can avoid directly using simdutf.
// Deliberately uses std::span to avoid having a circular dependency on //base.
#ifndef BASE_SIMDUTF_SHIM_H_
#define BASE_SIMDUTF_SHIM_H_
```

Good:

```cpp
// Shim so the //base GN target can avoid directly using simdutf.
//
// Do not use directly. Use APIs in base/base64.h instead.
//
// Deliberately uses std::span to avoid having a circular dependency on //base.
#ifndef BASE_SIMDUTF_SHIM_H_
#define BASE_SIMDUTF_SHIM_H_
```
