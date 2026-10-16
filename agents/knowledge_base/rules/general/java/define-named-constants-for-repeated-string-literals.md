---
name: general/define-named-constants-for-repeated-string-literals
rubric: readability
severity: should_fix
---

**Rule.** Define centralized named constants (such as inline constexpr char[] /
string_view in shared headers under namespace switches, or scoped enums/structs)
for repeated string literals, command-line switches, interface names, or
property keys across definitions and call sites. Do not scatter raw string
literals across producer and consumer call sites.

**Why.** Hardcoding raw string literals across multiple call sites is
error-prone, introduces typo risks, breaks symbol indexing and cross-referencing
in Code Search, and complicates refactoring. Centralizing string literals into
named constants establishes a single source of truth and enables compiler
verification.

## Examples

Bad:

```cpp
// Hardcoded command-line switches across producer and consumer:
command_line.AppendSwitchASCII("browser-version", GetBrowserVersion());
const std::wstring version = args.GetSwitchValueNative("browser-version");

// Hardcoded string literals across definitions and call sites:
let managerStatic = b.createNamedVariable(forBuiltin: "blink.mojom.LockManager")
let manager = b.callMethod("getRemote", on: managerStatic, withArgs: [])
```

Good:

```cpp
// In a shared header (e.g. under namespace switches):
namespace switches {
inline constexpr char kBrowserVersionSwitch[] = "browser-version";
}
command_line.AppendSwitchASCII(switches::kBrowserVersionSwitch, GetBrowserVersion());
args.AppendSwitchNative(switches::kBrowserVersionSwitch, browser_version);

// Centralized constants / enums:
private enum MojoStrings { static let lockManager = "blink.mojom.LockManager" }
private enum MojoMethods { static let getRemote = "getRemote" }
let managerStatic = b.createNamedVariable(forBuiltin: MojoStrings.lockManager)
let manager = b.callMethod(MojoMethods.getRemote, on: managerStatic, withArgs: [])
```
