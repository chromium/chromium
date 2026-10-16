---
name: general/use-visible-insets-when-calculating-keyboard-height
rubric: correctness
severity: should_fix
---

**Rule.** When calculating visible soft keyboard (IME) height or screen
occlusion from WindowInsetsCompat (or WindowInsets), subtract only visible
insets using getInsets(...) rather than getInsetsIgnoringVisibility(...).

**Why.** getInsetsIgnoringVisibility(...) returns inset dimensions even when the
system or navigation bar is currently hidden. Subtracting hidden insets
incorrectly underestimates the visible keyboard height when system bars are not
occupying screen space.

## Examples

Bad:

```cpp
int imeHeight = windowInsetsCompat.getInsets(WindowInsetsCompat.Type.ime()).bottom;
int bottomNavBars =
        windowInsetsCompat.getInsetsIgnoringVisibility(
                        WindowInsetsCompat.Type.navigationBars())
                .bottom;
return Math.max(0, imeHeight - bottomNavBars);
```

Good:

```cpp
int imeHeight = windowInsetsCompat.getInsets(WindowInsetsCompat.Type.ime()).bottom;
int bottomSystemBars =
        windowInsetsCompat.getInsets(WindowInsetsCompat.Type.systemBars()).bottom;
return Math.max(0, imeHeight - bottomSystemBars);
```
