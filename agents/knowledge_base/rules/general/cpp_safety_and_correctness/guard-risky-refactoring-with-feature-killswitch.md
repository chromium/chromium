---
name: general/guard-risky-refactoring-with-feature-killswitch
rubric: correctness
severity: should_fix
---

**Rule.** When landing potentially risky behavioral changes, bug fixes, OS API
migrations, or subsystem refactorings, guard the new implementation behind an
enabled-by-default base::Feature kill switch and retain the legacy behavior as a
fallback. Do not alter low-level OS, filesystem, or core subsystem semantics
unconditionally without a remote kill switch to disable the change if
regressions occur in the field.

**Why.** Deep low-level OS and subsystem changes (such as filesystem traversal,
COM interfaces, or permission semantics) carry risks of breaking atypical
environments or enterprise configurations. Providing an enabled-by-default
base::Feature kill switch allows the change to be disabled remotely via Finch
without requiring an emergency binary release or revert.

## Examples

Bad:

```cpp
// Direct API migration without fallback:
bool InvokeShellExecute(...) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  base::win::ShellExecuteOptions options{
      .verb = verb, .current_directory = working_directory};
  return SUCCEEDED(
      base::win::RunShellExecuteViaExplorer(resolved_path, args, options));
}

// Low-level filesystem behavior change without a kill switch:
if (recursive && !(attr & FILE_ATTRIBUTE_REPARSE_POINT)) {
  const DWORD error_code =
      DeleteFileRecursive(path, FILE_PATH_LITERAL("*"), true);
}
```

Good:

```cpp
BASE_FEATURE(kLaunchShellExecuteViaExplorer, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPreventReparsePointTraversal, base::FEATURE_ENABLED_BY_DEFAULT);

// API migration guarded with legacy fallback:
bool InvokeShellExecute(...) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  if (!base::FeatureList::IsEnabled(kLaunchShellExecuteViaExplorer)) {
    return InvokeShellExecuteOld(path, working_directory, args, verb,
                                 class_name, mask);
  }
  base::win::ShellExecuteOptions options{
      .verb = verb, .current_directory = working_directory};
  return SUCCEEDED(
      base::win::RunShellExecuteViaExplorer(resolved_path, args, options));
}

// Filesystem traversal change guarded with kill switch:
if (recursive && (!(attr & FILE_ATTRIBUTE_REPARSE_POINT) ||
                  !base::FeatureList::IsEnabled(
                      base::features::kPreventReparsePointTraversal))) {
  const DWORD error_code =
      DeleteFileRecursive(path, FILE_PATH_LITERAL("*"), true);
}
```
