---
name: general/prefer-base-nullcallback-over-bare-braces
rubric: readability
severity: nit
---

**Rule.** When passing an intentionally null or disengaged callback
(base::OnceCallback or base::RepeatingCallback) as an argument, use
base::NullCallback() (or base::NullCallbackAs<Signature>() when overload
disambiguation is needed) from base/functional/callback_helpers.h instead of
bare braces {}.

**Why.** Passing bare braces {} relies on implicit default construction, which
obscures caller intent and parameter types at call sites. Using
base::NullCallback() explicitly signals that a disengaged callback is
intentionally supplied (analogous to nullptr or std::nullopt) and prevents
confusion with active no-op callbacks like base::DoNothing().

## Examples

Bad:

```cpp
// Ambiguous and obscures callback parameter type at call site.
PickleIterator iter(pickle);
HistogramBase* deserialized = DeserializeHistogramInfo(&iter, {});
```

Good:

```cpp
#include "base/functional/callback_helpers.h"

// Explicitly conveys that an intentionally null callback is passed.
PickleIterator iter(pickle);
HistogramBase* deserialized =
    DeserializeHistogramInfo(&iter, base::NullCallback());
```
