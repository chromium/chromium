---
name: general/avoid-duplicate-constants-across-layers
rubric: design
severity: nit
---

**Rule.** Do not duplicate numeric, string, or configuration literals
representing the same semantic constant across different classes, modules, or
layers. Define a single source of truth in the lower-level component and
reference it in dependent consumers; if architectural boundaries or circular
dependencies prevent direct referencing, explicitly document in comments that
the constants must stay in sync.

**Why.** Duplicating identical literal values for the same semantic threshold
across components creates maintenance overhead and risks silent drift when one
copy is updated but others are missed. Referencing a single source of truth or
documenting synchronization invariants prevents inconsistencies.

## Examples

Bad:

```cpp
// In DeviceInfo.java (base/):
private static final int SCREEN_WIDTH_600_DP = 600;

// In DeviceFormFactor.java (ui/):
public static final int MINIMUM_TABLET_WIDTH_DP = 600;
```

Good:

```cpp
// In DeviceInfo.java (base/):
public static final int LARGE_DISPLAY_MIN_SCREEN_WIDTH_600_DP = 600;

// In DeviceFormFactor.java (ui/):
public static final int MINIMUM_TABLET_WIDTH_DP =
        DeviceInfo.LARGE_DISPLAY_MIN_SCREEN_WIDTH_600_DP;
```
