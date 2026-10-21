---
name: base/use-base-self-deleting-for-self-deleting-classes
rubric: safety
severity: optional
---

**Rule.** For classes that manage their own lifetime via self-deletion
(`delete this;`), inherit from `base::SelfDeleting`, accept
`base::SelfDeletingPassKey` as the last constructor parameter, keep the
destructor `private`, and instantiate instances using
`base::MakeSelfDeleting<T>(...)` instead of bare `new`. Do not expose a public
destructor or allow direct instantiation outside `base::MakeSelfDeleting`.

**Why.** Chromium style avoids bare calls to `new`. Inheriting from
`base::SelfDeleting` and requiring `base::SelfDeletingPassKey` prevents callers
from instantiating the class directly with `new` or allocating it on the stack,
while keeping the destructor private prevents external callers from directly
deleting the object.

## Examples

Bad:

```cpp
class DllPreReader : public base::PlatformThread::Delegate {
 public:
  explicit DllPreReader(const base::FilePath& module) : module_(module) {}
  void ThreadMain() override {
    base::PreReadFile(module_, /*is_executable=*/true, /*sequential=*/false);
    delete this;
  }
 private:
  ~DllPreReader() override = default;
};

// Bad: Instantiating a self-deleting class using bare new:
base::PlatformThread::CreateNonJoinableWithType(
    0, new DllPreReader(module), base::ThreadType::kDefault);
```

Good:

```cpp
#include "base/memory/self_deleting.h"

class DllPreReader : public base::PlatformThread::Delegate,
                     public base::SelfDeleting {
 public:
  DllPreReader(const base::FilePath& module, base::SelfDeletingPassKey key)
      : SelfDeleting(key), module_(module) {}
  void ThreadMain() override {
    base::PreReadFile(module_, /*is_executable=*/true, /*sequential=*/false);
    delete this;
  }
 private:
  ~DllPreReader() override = default;
};

// Good: Instantiated safely via base::MakeSelfDeleting:
base::PlatformThread::CreateNonJoinableWithType(
    0, base::MakeSelfDeleting<DllPreReader>(module),
    base::ThreadType::kDefault);
```
