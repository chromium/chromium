---
name: general/document-manual-verification-steps-in-commit-message
rubric: readability
severity: nit
---

**Rule.** When a change is verified manually, relies on observed runtime values,
or lacks feasible automated tests (such as build configuration or toolchain
adjustments), document how the issue was identified and verified in the commit
message body or a `Test:` footer. Format executed test targets and verification
steps as a concise narrative summary or condensed `Test:` footer line, rather
than pasting raw multi-line command blocks or terminal logs.

**Why.** Commit messages serve as the permanent historical record. Describing
how defects or missing dependencies were discovered and verified ensures
reviewers and future maintainers understand the fix when automated tests are
impractical, while concise narrative summaries and standard `Test:` footers
prevent git log clutter.

## Examples

Bad:

```cpp
// Anti-pattern 1: Omitting discovery and manual verification context for build/tool adjustments
[Build] Switch deps to data_deps in chromium_builder_asan

content_shell and d8 are needed for clusterfuzz and switching from deps
to data_deps will show them as runtime_deps of chromium_builder_asan in
gn desc.

Bug: 498605824

// Anti-pattern 2: Pasting multi-line shell command blocks with Markdown code fences
Tests:
```

autoninja -C out/Default base_unittests out/Default/base_unittests
--gtest_filter=FileUtilTest.DeletePathRecursively_DoesNotFollowJunctions

```

// Anti-pattern 3: Missing manual verification details for unautomated runtime fix
Fix Android ChildProcessConnection FallbackService histogram.

Bug: 475852547
```

Good:

```cpp
// Pattern 1: Describing issue discovery and verification steps in the description body
[Build] Switch deps to data_deps in chromium_builder_asan

content_shell and d8 are needed for clusterfuzz and switching from deps
to data_deps will show them as runtime_deps of chromium_builder_asan in
gn desc. Found that these were missing by running `gn desc <out>
//:chromium_builder_asan runtime_deps` and grepping for binaries.
Tested that they appear in runtime_deps with these changes.

Bug: 498605824

// Pattern 2: Condensed Test: footer line
Bug: 447871698
Test: base_unittests FileUtilTest.{Delete*Junctions*,IsLink}

// Pattern 3: Documented manual verification steps using Test: footer
Fix Android ChildProcessConnection FallbackService histogram.

Bug: 475852547
Test: Verified locally that serviceName evaluates to JAVA_SANDBOXED
for the class org.chromium.content.app.SandboxedProcessService0.
```
