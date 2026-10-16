---
name: general/avoid-upward-architectural-dependencies
rubric: design
severity: blocking
---

**Rule.** Do not reference, include, or depend on types, headers, or build
targets from higher-level architectural layers in lower-level modules (e.g.,
//base must not depend on //content, and //content must not depend on //chrome).
Additionally, do not define higher-layer concepts or enumerations (such as WebUI
or Extension process types) or hardcode names, constants, or special-case logic
for higher-layer components (such as //chrome file names) in lower-layer
libraries; parameterize the utility or use dependency injection/observer
interfaces instead.

**Why.** Chromium enforces strict downward architectural layering (//chrome ->
//content -> //base). Introducing upward dependencies, hardcoding higher-layer
concepts, or defining embedder-specific enums in lower-level libraries violates
modularity, creates circular dependencies, breaks standalone builds, and
prevents utility reuse.

## Examples

Bad:

```cpp
// In base/files/important_file_writer.cc:
// Hardcoding a higher-layer (//chrome) file name in //base is a layering violation:
if (path.BaseName().value() == FILE_PATH_LITERAL("Local State")) {
  tmp_file = CreateAndOpenLocalStateTemporaryFileInDir(path.DirName(), &tmp_file_path);
} else {
  tmp_file = CreateAndOpenTemporaryFileInDir(path.DirName(), &tmp_file_path);
}

// In base/process/current_process.h or base/tracing/protos/chrome_enums.proto:
// Defining embedder-specific (//chrome or //content) process types in //base:
enum class ShortProcessType { kRendererTopWebUI = 13 };
case CurrentProcessType::PROCESS_RENDERER_TOP_WEBUI:
  return "WebUI Top Renderer";

// In base/android/java/src/.../ChildProcessConnection.java:
// ChildProcessImportance is defined in //content, causing an upward layering dependency:
public void start(@ChildProcessImportance int initialImportance, ServiceCallback serviceCallback) { ... }
```

Good:

```cpp
// In base/files/important_file_writer.cc:
// Keep //base utility generic by parameterizing file name or flags:
File tmp_file = CreateAndOpenTemporaryFileInDir(
    path.DirName(), &tmp_file_path, /*additional_flags=*/0,
    path.BaseName().value());

// Keep higher-layer process types in //content or //chrome, injecting them generically:
enum class ProcessType { kRenderer, ... };

// In base/android/java/src/.../ChildProcessConnection.java:
// Keep parameter typed to base enum (ChildBindingState):
public void start(@ChildBindingState int initialBindingState, ServiceCallback serviceCallback) {
  assert initialBindingState != ChildBindingState.UNBOUND;
}
```
