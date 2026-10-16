---
name: general/resolve-main-bundle-id-across-macos-process-hierarchy
rubric: correctness
severity: should_fix
---

**Rule.** When identifying an application or resolving its bundle identifier
from a process ID (pid_t) or window owner on macOS, traverse the process
hierarchy (or resolve the responsible parent PID) to locate the root application
bundle. Do not rely solely on \[NSRunningApplication
runningApplicationWithProcessIdentifier:pid\].bundleIdentifier on an arbitrary
PID, as multi-process applications (such as Chromium or Electron) spawn child
and helper processes that lack a main bundle identifier or have helper-specific
sub-bundle IDs.

**Why.** Multi-process applications on macOS run helper, renderer, and utility
processes that either lack a bundleIdentifier or return a helper-specific
sub-bundle identifier. Directly querying NSRunningApplication on a child process
or window PID fails to identify the owning application or breaks permissions and
system integration checks unless the parent process chain is traversed to
resolve the root application.

## Examples

Bad:

```cpp
std::optional<std::string> GetBundleIdForProcess(pid_t pid) {
  // Directly querying NSRunningApplication on a child/helper PID
  // fails to resolve the root application bundle for multi-process apps.
  NSRunningApplication* app =
      [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
  if (!app || !app.bundleIdentifier) {
    return std::nullopt;
  }
  return base::SysNSStringToUTF8(app.bundleIdentifier);
}
```

Good:

```cpp
std::optional<std::string> GetBundleIdForProcess(pid_t pid) {
  // Traverse the process hierarchy to locate the root application with a bundle ID.
  while (pid > 0) {
    NSRunningApplication* app =
        [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    if (app && app.bundleIdentifier) {
      return base::SysNSStringToUTF8(app.bundleIdentifier);
    }
    pid = GetParentProcessId(pid);
  }
  return std::nullopt;
}
```
