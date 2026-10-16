---
name: general/avoid-redundant-directory-prefixes-in-filenames
rubric: readability
severity: should_fix
---

**Rule.** Do not add redundant path or platform qualifications when naming
source or test files. Specifically:

1. Do not prefix filenames with the enclosing directory or platform name when
   the directory hierarchy already provides context (e.g., prefer
   `base/win/delayload_helpers.h` over `base/win/win_delayload_helpers.h`).
2. Do not combine redundant, overlapping, or hierarchical platform suffixes
   (such as `_apple_ios_` or `_posix_linux_`) in filenames. Use either the
   specific target platform suffix (e.g., `_ios_unittest.mm`) or the broader
   platform family suffix (e.g., `_apple_unittest.mm`) with internal
   `#if BUILDFLAG(IS_...)` guards.

**Why.** Chromium's directory structure and GN build filter patterns already
organize and qualify files by subsystem and target platform. Duplicating
directory names as prefixes or chaining hierarchical platform suffixes creates
redundant, awkward filenames, lengthens `#include` paths, and clutters the
codebase.

## Examples

Bad:

```cpp
// File: base/win/win_delayload_helpers.h (redundant directory prefix)
#ifndef BASE_WIN_WIN_DELAYLOAD_HELPERS_H_
#define BASE_WIN_WIN_DELAYLOAD_HELPERS_H_
namespace base::win { /* ... */ }
#endif

// File: base/message_loop/message_pump_apple_ios_unittest.mm (chained suffixes)
#include "base/message_loop/message_pump_apple.h"
#include "base/message_loop/message_pump_ios.h"
```

Good:

```cpp
// File: base/win/delayload_helpers.h (concise component name)
#ifndef BASE_WIN_DELAYLOAD_HELPERS_H_
#define BASE_WIN_DELAYLOAD_HELPERS_H_
namespace base::win { /* ... */ }
#endif

// File: base/message_loop/message_pump_ios_unittest.mm (single platform suffix)
#include "base/message_loop/message_pump_ios.h"
```
