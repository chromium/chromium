---
name: general/zero-initialize-c-struct-out-parameters
rubric: safety
severity: should_fix
---

**Rule.** When declaring local C-style aggregate structs or descriptor objects
to pass as out-parameters to external, OS, or C platform APIs, defensively
zero-initialize them at declaration (e.g., `Type desc = {};`). Do not leave them
default-initialized without an initializer, especially when returning the struct
from a getter or helper function.

**Why.** C-style aggregate structs have indeterminate values when
default-initialized in C++. If the external C API call fails, only partially
populates the structure, or is bypassed in release builds when protected by
debug assertions like DCHECK, subsequent reads or return statements result in
uninitialized memory reads.

## Examples

Bad:

```cpp
AHardwareBuffer_Desc ScopedHardwareBufferHandle::Describe() const {
  DCHECK(buffer_);
  AHardwareBuffer_Desc desc;
  AHardwareBuffer_describe(buffer_, &desc);
  return desc;
}
```

Good:

```cpp
AHardwareBuffer_Desc ScopedHardwareBufferHandle::Describe() const {
  DCHECK(buffer_);
  AHardwareBuffer_Desc desc = {};
  AHardwareBuffer_describe(buffer_, &desc);
  return desc;
}
```
