---
name: general/reuse-dedicated-accessors-in-composite-functions
rubric: design
severity: should_fix
---

**Rule.** When populating composite data structures (such as system, device, or
hardware info structs) or implementing aggregate functions, call existing or
dedicated getter/accessor methods rather than duplicating calculations, value
extraction routines, or hardcoded literal values inline. Delegate composite
field assignments directly to the corresponding dedicated accessor to maintain a
single source of truth.

**Why.** Duplicating hardcoded literal values, calculation logic, or platform
extraction routines across multiple functions increases maintenance overhead and
introduces inconsistency risks if one location is updated while another is
overlooked. Reusing dedicated accessors ensures consistent behavior and
single-point maintainability across all callers.

## Examples

Bad:

```cpp
// BAD: Duplicating hardcoded string literals or extraction logic inline
// instead of reusing the dedicated accessor method.
// static
std::string SysInfo::HardwareManufacturer() {
  return "Apple Inc.";
}

// static
SysInfo::HardwareInfo SysInfo::GetHardwareInfoSync() {
  HardwareInfo info;
  info.manufacturer = "Apple Inc.";
  info.model = HardwareModelName();
  return info;
}
```

Good:

```cpp
// GOOD: Delegate composite field assignments directly to dedicated accessors.
// static
std::string SysInfo::HardwareManufacturer() {
  return "Apple Inc.";
}

// static
SysInfo::HardwareInfo SysInfo::GetHardwareInfoSync() {
  HardwareInfo info;
  info.manufacturer = HardwareManufacturer();
  info.model = HardwareModelName();
  return info;
}
```
