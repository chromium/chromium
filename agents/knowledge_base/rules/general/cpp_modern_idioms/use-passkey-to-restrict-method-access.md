---
name: general/use-passkey-to-restrict-method-access
rubric: readability
severity: should_fix
---

**Rule.** When restricting access to sensitive, internal, or test-only methods
and constructors to specific authorized classes, require a
base::PassKey<AllowedCaller> parameter rather than leaving methods
unconditionally public or declaring broad friend classes. Leave the
base::PassKey parameter unnamed in declarations and implementations to indicate
it carries no runtime data.

**Why.** Unconditionally public methods can be called accidentally from
unauthorized call sites, violating architectural boundaries and class
invariants. While friend declarations grant unrestricted access to all private
internals, base::PassKey<T> enforces fine-grained, compile-time access control
strictly on designated methods without runtime overhead.

## Examples

Bad:

```cpp
class FeatureList {
 public:
  // Sensitive method exposed unconditionally or relying on broad friend:
  void DisableRuntimeMutableFeature(std::string_view feature_name);

  // Friending entire caller/test class exposes all private internals:
  friend class FeatureListTest;
};
```

Good:

```cpp
#include "base/types/pass_key.h"

class FeatureList {
 public:
  // Access restricted strictly to FeatureListTest via PassKey:
  void DisableRuntimeMutableFeature(base::PassKey<FeatureListTest>,
                                    std::string_view feature_name);
};
```
