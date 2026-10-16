---
name: general/omit-drawing-instruction-comments-in-vector-icons
rubric: readability
severity: nit
---

**Rule.** In `.icon` vector icon files, list drawing commands contiguously
within each icon definition without blank lines between operations or inline
comments describing individual drawing instructions or paths. Keep `.icon` files
as compact, raw command sequences matching automated generator output.

**Why.** Vector `.icon` files are compiled into C++ array definitions and
generated from SVGs using automated conversion tools (such as Skiafy or GM3
repositories). Developers do not manually maintain or read individual
coordinates, making inline narration comments and superfluous blank lines
unnecessary formatting noise that easily desynchronizes when icons are
regenerated.

## Examples

Bad:

```cpp
// Top horizontal line
MOVE_TO, 2.9f, 7.8f,
LINE_TO, 5.0f, 7.8f,

// Arrowhead
MOVE_TO, 10.0f, 15.0f,
LINE_TO, 15.0f, 10.0f,
CLOSE,
```

Good:

```cpp
CANVAS_DIMENSIONS, 20,
MOVE_TO, 2.9f, 7.8f,
LINE_TO, 5.0f, 7.8f,
MOVE_TO, 10.0f, 15.0f,
LINE_TO, 15.0f, 10.0f,
CLOSE,
```
