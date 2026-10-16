---
name: general/place-explanatory-comments-above-aidl-declarations
rubric: readability
severity: nit
---

**Rule.** In AIDL interface and parcelable definitions (`.aidl`), place
explanatory comments on their own line directly preceding the associated field
or method declaration rather than as trailing inline comments at the end of the
line.

**Why.** AIDL files are not processed by automated code formatters like
`clang-format`. Placing comments on preceding lines prevents excessively long
lines, avoids awkward spacing and alignment drift, and maintains consistent
readability across interface definitions.

## Examples

Bad:

```cpp
parcelable DeviceInfo {
  // Trailing comments create overly long lines and inconsistent spacing in AIDL files.
  boolean wasLaunchedOnLargeDisplay; // Display width is >= 600dp. At this width, desktop pages are shown.
  int displayDensityDpi; // Screen density in dots per inch.
}
```

Good:

```cpp
parcelable DeviceInfo {
  // Display width is >= 600dp. At this width, desktop pages are shown.
  boolean wasLaunchedOnLargeDisplay;

  // Screen density in dots per inch.
  int displayDensityDpi;
}
```
