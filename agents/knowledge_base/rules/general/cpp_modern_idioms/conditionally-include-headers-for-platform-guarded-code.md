---
name: general/conditionally-include-headers-for-platform-guarded-code
rubric: readability
severity: should_fix
---

**Rule.** When a header is only required to support platform-guarded
declarations, definitions, or types (e.g. inside #if BUILDFLAG(IS\_...)),
conditionally include that header under the corresponding #if BUILDFLAG(...)
guard rather than including it unconditionally.

**Why.** Unconditionally including headers that are only needed on specific
platforms unnecessarily inflates compiler input size and compilation times on
all other platforms, and can introduce unwanted transitive dependencies or
symbol pollution.

## Examples

Bad:

```cpp
#include "base/auto_reset.h"
#include "base/base_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
[[nodiscard]] BASE_EXPORT AutoReset<bool> SetIsEnterpriseDeviceForTesting(
    bool is_enterprise);
#endif  // BUILDFLAG(IS_WIN)
```

Good:

```cpp
#include "base/base_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/auto_reset.h"
#endif

#if BUILDFLAG(IS_WIN)
[[nodiscard]] BASE_EXPORT AutoReset<bool> SetIsEnterpriseDeviceForTesting(
    bool is_enterprise);
#endif  // BUILDFLAG(IS_WIN)
```
