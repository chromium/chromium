---
name: general/motivate-precursor-apis-and-link-follow-up-cl
rubric: readability
severity: should_fix
---

**Rule.** When introducing new APIs, helper methods, classes, or scaffolding
that currently lack call sites within the CL, either combine the changes with
the consuming CL or clearly document why they are being added in the commit
description and link the follow-up consumer CL (e.g., using
`https://crrev.com/c/<id>`).

**Why.** Adding new APIs or helper functions in isolation without in-CL callers
makes it difficult for reviewers and git history readers to verify their design,
understand why the code exists, or confirm how it will be integrated. Motivating
the addition and linking the follow-up CL provides immediate context and
prevents landing orphaned or premature code.

## Examples

Bad:

```cpp
Add SysInfo::HardwareManufacturerName()

This change introduces a new static method
`SysInfo::HardwareManufacturerName()` to synchronously retrieve the
hardware manufacturer name. The implementation reuses the existing
`GetHardwareInfoSync()` mechanism.

Bug: 123456
```

Good:

```cpp
Add SysInfo::HardwareManufacturerName()

This change introduces a new static method
`SysInfo::HardwareManufacturerName()` to synchronously retrieve the
hardware manufacturer name. The implementation reuses the existing
`GetHardwareInfoSync()` mechanism.

This data is required for the new hardware_manufacturer Finch filter.
The variations-specific filtering logic is implemented in a follow-up CL
in the same relation chain.

Follow-up CL:
https://crrev.com/c/7809609

Bug: 123456
```
