---
name: ios/use-std-function-for-swift-callback-interop
rubric: readability
severity: should_fix
---

**Rule.** When passing callbacks across the C++/Swift boundary, use
std::function via base::swift_helpers::ToStdFunction to wrap base::OnceCallback
and base::RepeatingCallback rather than building custom wrapper structs and
shims.

**Why.** Although std::function is generally banned in Chromium, C++/Swift
interop is an approved exception. Wrapping base::Callback in std::function
allows Swift to invoke closures directly using standard function call syntax
without requiring custom wrapper structs and helper dispatch functions.

## Examples

Bad:

```cpp
// Custom wrapper structs and shims to transport callbacks across Swift boundary:
SWIFT_DECLARE_INTEROP_WRAPPER(CxxRepeatingClosure, base::RepeatingClosure)
SWIFT_DECLARE_MOVE_ONLY_INTEROP_WRAPPER(CxxOnceClosure, base::OnceClosure)

class ClosureProvider {
 public:
  CxxRepeatingClosure* GetRepeatingClosure();
  CxxOnceClosure* GetOnceClosure();
};
void RunCxxRepeatingClosure(CxxRepeatingClosure* cb);
void RunCxxOnceClosure(CxxOnceClosure* cb);
```

Good:

```cpp
#include "base/apple/swift_callback_helpers.h"

class ClosureProvider {
 public:
  virtual std::function<void()> GetRepeatingClosure() = 0;
  virtual std::function<void()> GetOnceClosure() = 0;
};

// In implementation:
std::function<void()> ClosureProviderImpl::GetRepeatingClosure() {
  base::RepeatingClosure cb = base::BindRepeating(&ClosureProviderImpl::Callback, ...);
  return base::swift_helpers::ToStdFunction(std::move(cb));
}
```
