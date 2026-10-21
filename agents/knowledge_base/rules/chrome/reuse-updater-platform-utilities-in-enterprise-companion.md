---
name: chrome/reuse-updater-platform-utilities-in-enterprise-companion
rubric: design
severity: should_fix
---

**Rule.** In `chrome/enterprise_companion`, do not duplicate platform, operating
system, or security utility routines locally. Reuse shared utility
implementations from `chrome/updater/util/` (such as `win_util.h`, `mac_util.h`,
and `posix_util.h`, including helpers like `updater::GetLoggedOnUserToken()`).

**Why.** `chrome/enterprise_companion` was decoupled from `chrome/updater` and
explicitly permits dependencies on `chrome/updater` in `DEPS`. Duplicating
complex OS and security utilities (such as session enumeration, user access
token retrieval from `explorer.exe`, or security descriptors) creates redundant
code, maintenance overhead, and behavioral drift across the companion app and
the updater.

## Examples

Bad:

```cpp
// Duplicating WTS session enumeration and token extraction locally:
std::optional<base::win::AccessToken> GetLoggedOnUserToken() {
  DWORD console_session = ::WTSGetActiveConsoleSessionId();
  if (console_session == 0xFFFFFFFF) {
    return std::nullopt;
  }
  // Custom token extraction and session inspection logic...
  return std::nullopt;
}
```

Good:

```cpp
#include "chrome/updater/util/win_util.h"

// Reuse the canonical updater utility:
std::optional<base::win::AccessToken> token = updater::GetLoggedOnUserToken();
```
