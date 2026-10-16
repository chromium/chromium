---
name: general/keep-paired-helpers-in-same-module
rubric: design
severity: should_fix
---

**Rule.** When extracting, refactoring, or moving paired or complementary helper
functions (such as getters and setters, readers and writers, or encoders and
decoders) into a shared or internal helper module, move their counterparts into
the same module rather than leaving them split across local files or anonymous
namespaces.

**Why.** Splitting paired operations across different locations—such as placing
a getter in an internal helper module while leaving its setter as a local
anonymous-namespace function—leads to asymmetric and fragmented architecture,
impairs maintainability, and complicates handling platform-specific variants
consistently.

## Examples

Bad:

```cpp
// base/process/environment_internal.h
BASE_EXPORT base::span<const char* const> GetEnvironment();

// base/process/launch_posix.cc (anonymous namespace)
void SetEnvironment(char** env) {
  environ = env;
}
```

Good:

```cpp
// base/process/environment_internal.h
BASE_EXPORT base::span<const char* const> GetEnvironment();
BASE_EXPORT void SetEnvironment(char** env);

// base/process/environment_internal.cc
void SetEnvironment(char** env) {
#if BUILDFLAG(IS_APPLE)
  *_NSGetEnviron() = env;
#else
  environ = env;
#endif
}
```
