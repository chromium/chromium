---
name: general/prefer-unified-cross-platform-implementation
rubric: readability
severity: should_fix
---

**Rule.** Prefer a unified cross-platform implementation over
platform-conditional `#if BUILDFLAG(...)` branches when logic can be shared
safely across all platforms. Do not arbitrarily restrict fixes, improvements, or
parameters to specific OS targets (such as desktop platforms) unless required by
platform-specific technical constraints or platform incompatibilities.

**Why.** Unnecessary platform conditionals fragment test coverage, increase
cognitive load, and cause code paths to diverge across operating systems.
Keeping shared logic unified ensures consistent behavior and reduces maintenance
overhead across all supported platforms.

## Examples

Bad:

```cpp
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  tmp_file = CreateAndOpenTemporaryFileInDirWithPrefix(
      path.DirName(), path.BaseName().value(), &tmp_file_path);
#else
  tmp_file = CreateAndOpenTemporaryFileInDir(path.DirName(), &tmp_file_path);
#endif
```

Good:

```cpp
  File tmp_file = CreateAndOpenTemporaryFileInDir(
      path.DirName(), &tmp_file_path, /*additional_flags=*/0,
      path.BaseName().value());
```
