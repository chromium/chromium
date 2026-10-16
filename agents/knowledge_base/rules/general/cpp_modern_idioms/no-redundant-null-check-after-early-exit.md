---
name: general/no-redundant-null-check-after-early-exit
rubric: readability
severity: nit
---

**Rule.** Do not add redundant null checks or defensive pointer guards in loop
conditions or subsequent expressions when preceding control flow or early
returns already guarantee that the pointer is non-null.

**Why.** Redundant defensive null checks add visual clutter and mislead readers
by suggesting that a pointer might be null at a point where earlier control flow
has already established its validity.

## Examples

Bad:

```cpp
if (sd->dacl()->is_null()) {
  return true;
}
PACL pacl = sd->dacl()->get();
for (DWORD ace_index = 0; pacl && ace_index < pacl->AceCount; ++ace_index) {
  // ...
}
```

Good:

```cpp
if (sd->dacl()->is_null()) {
  return true;
}
PACL pacl = sd->dacl()->get();
for (DWORD ace_index = 0; ace_index < pacl->AceCount; ++ace_index) {
  // ...
}
```
