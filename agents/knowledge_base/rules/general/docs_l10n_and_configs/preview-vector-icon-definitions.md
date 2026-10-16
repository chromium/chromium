---
name: general/preview-vector-icon-definitions
rubric: readability
severity: should_fix
---

**Rule.** When creating or updating .icon vector icon definitions, source
standardized icon definitions from official repositories (such as
go/chrome-gm3-icons) to ensure consistent stroke weights, proportions, and
canvas bounds, and preview the rendered icon using a vector icon preview tool
(such as Vector Icon Viewer, the Codesearch extension, or views_examples).
Verify that paths are properly closed, curves and tangents render smoothly
without distortion, and geometry aligns to the intended pixel grid across all
canvas dimensions.

**Why.** Ad-hoc SVG conversions and manual edits often produce vector icons with
non-standard stroke weights, distorted Bézier curves, inverted fill rules, or
misaligned canvas bounds. Sourcing official GM3 icon definitions ensures design
system compliance, while previewing rendered icons catches visual defects and
rasterization glitches before landing.

## Examples

Bad:

```cpp
CANVAS_DIMENSIONS, 16,
FILL_RULE_NONZERO,
MOVE_TO, 3.79f, 10.23f,
R_ARC_TO, 5.3f, 5.3f, 0, 0, 1, -0.43f, -1.06f,
ARC_TO, 4.3f, 4.3f, 0, 0, 1, 3.2f, 8,
R_QUADRATIC_TO, 0, -0.5f, 0.35f, -0.85f,
```

Good:

```cpp
CANVAS_DIMENSIONS, 16,
FILL_RULE_NONZERO,
MOVE_TO, 4.49f, 7.99f,
R_CUBIC_TO, 0, 0.13f, 0.01f, 0.25f, 0.02f, 0.38f,
R_CUBIC_TO, 0.02f, 0.12f, 0.03f, 0.25f, 0.05f, 0.36f,
CLOSE,
```
