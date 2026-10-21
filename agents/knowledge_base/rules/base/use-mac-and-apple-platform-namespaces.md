---
name: base/use-mac-and-apple-platform-namespaces
rubric: readability
severity: should_fix
---

**Rule.** When defining platform-specific utilities and types in `//base`, place
macOS-only code in `namespace base::mac` (under `base/mac/`) and iOS-only code
in `namespace base::ios` (under `base/ios/`). Reserve `namespace base::apple`
(under `base/apple/`) strictly for utilities shared across all Apple platforms
(both macOS and iOS).

**Why.** In `//base`, platform directories and namespaces reflect target
platform support. Placing macOS-only APIs (such as AppKit or macOS process
utilities) in `base::apple` misleads callers about iOS support and causes
compilation or runtime issues, while placing shared Apple utilities in
`base::mac` prevents cross-platform reuse.

## Examples

Bad:

```cpp
// In base/mac/appkit_util.h:
namespace base::apple {
// macOS-only AppKit / process utility placed in base::apple namespace
std::optional<std::string> GetBundleIdFromPid(pid_t pid);
}  // namespace base::apple
```

Good:

```cpp
// In base/mac/appkit_util.h:
namespace base::mac {
// macOS-only code placed in namespace base::mac under base/mac/
std::optional<std::string> GetBundleIdFromPid(pid_t pid);
}  // namespace base::mac

// In base/apple/foundation_util.h:
namespace base::apple {
// Code shared across macOS and iOS in namespace base::apple under base/apple/
std::string CFStringToUTF8(CFStringRef str);
}  // namespace base::apple
```
