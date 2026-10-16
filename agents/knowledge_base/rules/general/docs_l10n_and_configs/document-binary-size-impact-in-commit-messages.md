---
name: general/document-binary-size-impact-in-commit-messages
rubric: readability
severity: optional
---

**Rule.** When authoring a CL that causes noticeable binary size or compile-time
bloat (such as including widely-used headers, expanding templates, or triggering
Compile-Size / Binary-Size warnings), explicitly document the final official
binary size impact (such as the delta on an official optimized build of Chrome)
and the justification in the CL description rather than only citing intermediate
compile artifact growth.

**Why.** Intermediate compile artifact growth or preprocessed header expansion
does not reflect the actual shipping impact after compiler optimizations,
dead-code elimination, and linking. Documenting the measured official binary
size delta alongside the architectural or safety rationale allows reviewers and
size gardeners to properly evaluate the trade-offs and trace regressions.

## Examples

Bad:

```cpp
The overall size of compiled artifacts bloats 208 MiB with this change
(from including `bounds_check.h` in the widely-included `span.h`).

Bug: 484171909
Compile-Size: `bounds_check.h` is hard to logically break up any more.
```

Good:

```cpp
The overall size of compiled artifacts bloats 208 MiB with this change
(from including `bounds_check.h` in the widely-included `span.h`). The
size of the Chrome binary itself (with `is_official_build = true`) on
Linux increases 52 KiB:

1537432	out/vanilla-official-build/chrome
1537484	out/BoggedDown/chrome

Bug: 484171909
Compile-Size: `bounds_check.h` is hard to logically break up any more.
```
