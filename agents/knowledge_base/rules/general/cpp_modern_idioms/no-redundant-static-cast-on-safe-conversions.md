---
name: general/no-redundant-static-cast-on-safe-conversions
rubric: readability
severity: nit
---

**Rule.** Do not add redundant static_cast or explicit casting operations when a
variable or expression is already of the target type, or when standard implicit
conversion is safe and lossless (such as unsigned integer widening). Rely on the
existing variable type or standard implicit conversion rather than adding
explicit cast boilerplate.

**Why.** Explicitly casting an expression to its already established type or
across safe implicit widening conversions adds unnecessary visual clutter and
boilerplate without improving type safety, and can mislead readers into assuming
a narrowing conversion or downcast is occurring.

## Examples

Bad:

```cpp
// Redundant cast on variable already of target type:
UIWindowScene* scene = base::apple::ObjCCast<UIWindowScene>(connectedScene);
_window = [[UIWindow alloc]
    initWithWindowScene:static_cast<UIWindowScene*>(scene)];

// Redundant static_cast on safe implicit widening conversion:
descriptors[1].Size = static_cast<ULONG>(meta_data_index);
```

Good:

```cpp
// Pass variable directly without redundant cast:
UIWindowScene* scene = base::apple::ObjCCast<UIWindowScene>(connectedScene);
_window = [[UIWindow alloc] initWithWindowScene:scene];

// Rely on standard implicit widening conversion:
descriptors[1].Size = meta_data_index;
```
