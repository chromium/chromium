---
name: gin/use-v8flag-prefix-over-gin-feature
rubric: readability
severity: should_fix
---

**Rule.** Do not declare a dedicated base::Feature in gin/gin_features and wire
it via SetV8FlagsIfOverridden() in gin/v8_initializer.cc solely to toggle or
pass a V8 flag. Use the dynamic V8Flag\_\<flag_name> feature prefix (e.g.,
--enable-features=V8Flag_foo_bar) to forward flags to V8 without Chromium-side
feature boilerplate.

**Why.** gin/v8_initializer.cc dynamically strips the 'V8Flag\_' prefix from
features specified on the command line, in test suites, or in field trials and
forwards them directly to V8 as flags. Adding dedicated base::Feature
definitions in gin adds redundant boilerplate.

## Examples

Bad:

```cpp
// In gin/gin_features.h:
GIN_EXPORT BASE_DECLARE_FEATURE(kJavaScriptDeferPhaseImports);

// In gin/v8_initializer.cc:
SetV8FlagsIfOverridden(features::kJavaScriptDeferPhaseImports,
                       "--js-defer-import-eval", "--no-js-defer-import-eval");

// In VirtualTestSuites or command-line:
"args": ["--enable-features=JavaScriptDeferPhaseImports"]
```

Good:

```cpp
// No gin/gin_features.h or gin/v8_initializer.cc changes required.

// In VirtualTestSuites, field trials, or command-line flags:
"args": ["--enable-features=V8Flag_js_defer_import_eval"]
```
