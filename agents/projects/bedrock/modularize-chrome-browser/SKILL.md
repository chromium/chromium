---
name: modularize-chrome-browser
description: Modularize a chrome/browser/ subfolder by splitting its sources out of the monolithic //chrome/browser:browser target into dedicated source_set targets in the subfolder's own BUILD.gn. Use when the user asks to modularize, extract, or create BUILD targets for a chrome/browser/ subfolder, or mentions "Project Bedrock", "//chrome/browser modularization", or wants to split a subfolder into header/impl/test targets. Also use when the user says "modularize chrome/browser/X" for any X.
allowed-tools: Read
---

# Modularize chrome/browser/ Subfolder

Extract sources for a `chrome/browser/<subfolder>/` from the monolithic
`//chrome/browser:browser` target into well-defined `source_set` targets in the
subfolder's own `BUILD.gn`. Changes must conform to
`//docs/chrome_browser_design_principles.md`.

## Goal

The end state is: - `chrome/browser/<subfolder>/BUILD.gn` defines
`::<subfolder>` (headers), `::impl` (.cc files), `::unit_tests`, and
`::browser_tests` as needed. - `chrome/browser/BUILD.gn` no longer lists the
subfolder's `.cc`/`.h` files directly; instead it deps on `::impl`. -
`chrome/test/BUILD.gn` no longer lists the subfolder's test files directly;
instead it deps on `::unit_tests` / `::browser_tests`. - Build verification
passes.

**Important:** Do not make any changes unless explicitly instructed by the user.
When asked to plan, produce a numbered list of steps only.

## Phase 1: Inventory

### Check if modularization is actually needed

An existing `BUILD.gn` in the subfolder does **not** mean the subfolder is
already modularized. Verify by checking whether the files are still listed in
`chrome/browser/BUILD.gn`:

```bash
grep -n "<subfolder>/" chrome/browser/BUILD.gn | head -60
grep -n "<subfolder>/" chrome/test/BUILD.gn | head -60
```

If files appear there, modularization is needed regardless of whether a
`BUILD.gn` already exists.

### Determine if the subfolder is platform-specific or hybrid

```bash
ls chrome/browser/<subfolder>/
```

| Subfolder layout | Classification | Action | | ------------------------ |
----------------- | ---------------------------- | | Only an `android/` (or |
Platform-specific | Skip — nothing to modularize | | : other platform) : : at
the root level : | : sub-subfolder, no : : : | : `.cc`/`.h` at root : : : | Has
`.cc`/`.h` files at | Hybrid | Modularize the root | | : root AND a platform : :
`.cc`/`.h` files; leave the : | : sub-subfolder : : platform subfolder alone : |
Has `.cc`/`.h` files, no | Standard | Modularize normally | | : platform
subfolder : : :

### Categorize every file

| Category | Files | | ------------------------------------ |
------------------------------------ | | Public headers (`.h` that form the | →
`::<subfolder>` target | | : API surface) : : | Implementation (`.cc` + paired
`.h`) | → `::impl` target | | `*_unittest.cc` | → `::unit_tests` target | |
`*_browsertest.cc` or | → `::browser_tests` target | | : `*_browser_test.cc` : :
| Fuzzer (`*_fuzzer.cc`) | → separate `fuzzer_test()` (leave in | | : : place) :

Also read each header to understand which deps are actually required at the
header level vs. only in `.cc` files.

## Phase 2: Choose a target pattern

### Pattern A: Two-source-set (header + impl) — preferred for most cases

Use when there are meaningful `.h` files that consumers depend on independently
of the `.cc` files, or when circular dependencies need to be broken.

```gn
source_set("<subfolder>") {
  sources = [
    "foo.h",
    "foo_factory.h",
  ]
  public_deps = [
    # Only deps whose types appear in the public header signatures.
    "//base",
    "//chrome/browser/profiles:profile",
    "//components/keyed_service/core",
  ]
  # Deps used only by .cc files belong in ::impl, not here.
  # Keep public_deps minimal — they are transitive and leak to all consumers.
}

source_set("impl") {
  sources = [
    "foo.cc",
    "foo_factory.cc",
  ]
  public_deps = [ "//chrome/browser:browser_public_dependencies" ]
  deps = [
    ":<subfolder>",
    "//base",
    # Deps only needed in .cc files go here as private deps.
    "//content/public/browser",
  ]
}
```

### Pattern B: Single target with `public` + `sources` split

Use for smaller or simpler components where the header/impl split doesn't add
meaningful value. Declare public headers explicitly using the `public` variable:

```gn
source_set("<subfolder>") {
  public = [
    "foo.h",
  ]
  sources = [
    "foo.cc",
    "foo_factory.cc",
    "foo_factory.h",  # internal header, not part of public API
  ]
  public_deps = [
    "//base",
  ]
  deps = [
    "//content/public/browser",
  ]
}
```

### Choosing between patterns

- Prefer **Pattern A** when the component has a significant public API surface
  or when there are downstream circular-dependency constraints.
- Prefer **Pattern B** for simple, self-contained components with few consumers.
- In either case, keep `public_deps` to the strict minimum — they are transitive
  and increase the risk of circular dependencies for all downstream consumers.
  Move implementation-only deps to private `deps`.

## Phase 3: Write the new BUILD.gn

```gn
# Copyright 20XX The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# (import any needed buildflags, e.g.)
# import("//extensions/buildflags/buildflags.gni")

source_set("<subfolder>") {
  # ... (Pattern A or B from above)
}

source_set("impl") {
  # ... (Pattern A only)
}

source_set("unit_tests") {
  testonly = true
  sources = [ "foo_unittest.cc" ]
  deps = [
    ":<subfolder>",
    ":impl",
    "//base/test:test_support",
    "//chrome/test:test_support",
    "//testing/gtest",
  ]
}

source_set("browser_tests") {
  testonly = true
  defines = [ "HAS_OUT_OF_PROC_TEST_RUNNER" ]
  sources = [ "foo_browsertest.cc" ]
  deps = [
    ":<subfolder>",
    ":impl",
    "//base/test:test_support",
    "//chrome/test:test_support",
    "//content/test:test_support",
    "//testing/gtest",
  ]
}
```

**Key rules:**

- Omit targets that have no files (e.g., skip `::browser_tests` if there are no
  browser test files).
- `::browser_tests` always sets `defines = ["HAS_OUT_OF_PROC_TEST_RUNNER"]`.
- Preserve all platform-specific `if (is_android)`, `if (is_chromeos)`,
  `if (enable_extensions)` guards exactly as they appear in the original
  `chrome/browser/BUILD.gn`.
- Factory `.h` files belong in the interface/public target even when the `.cc`
  is in `::impl`.
- **Platform-specific dependencies:** Ensure that dependencies are wrapped in
  the same platform conditional blocks (e.g., `if (is_chromeos)`) as the files
  that use them.

### Platform-agnostic interface design

If a class uses an interface pattern (e.g., a delegate or checker injected via
constructor), do **not** add `BUILDFLAG(IS_ANDROID)` build guards around the
code just to skip it on desktop. Instead, pass `nullptr` on platforms that don't
have an implementation and add a null-check in the method body. This keeps the
code testable on all platforms and avoids ifdef fragmentation.

```cpp
// Good: platform-agnostic, null-safe
bool Foo::IsInteractable() {
  if (!interactability_checker_) return true;
  return interactability_checker_->Check();
}
```

## Phase 4: Update parent BUILD files

### chrome/browser/BUILD.gn

1. Remove each `.h` and `.cc` file from the monolithic `sources` list.
2. Add a dep on the new impl target: `gn "//chrome/browser/<subfolder>:impl",`
   (For Pattern B, dep on `"//chrome/browser/<subfolder>"` instead.)
3. If there was already a dep on `"//chrome/browser/<subfolder>"`, keep or
   replace it with `:impl` as appropriate.

### chrome/test/BUILD.gn

Remove `*_unittest.cc` files from `chrome_unit_tests` sources and add:
`gn "//chrome/browser/<subfolder>:unit_tests",`

Remove `*_browsertest.cc` files from browser test sources and add:
`gn "//chrome/browser/<subfolder>:browser_tests",`

## Phase 5: Fix GN include errors

Run build verification first (Phase 6), then resolve any errors.

### Strict Rules

- **Never use `// nogncheck`** to bypass header check failures. Fix the
  dependency graph instead.
  - *Exception*: The only acceptable case for `nogncheck` is to bypass
    conditional includes that are not currently supported by GN. In such cases,
    the added pragma MUST be exactly `// nogncheck crbug.com/40147906`.
- **Headers in Monolithic Targets:** If a file in your new module includes a
  header from another folder that is still part of a monolithic target (like
  `//chrome/test:unit_tests`), you cannot move that file to the module without
  violating header checks. In this case, leave the file in the monolith until
  the other folder is also modularized.

### Error type 1: "Include not allowed" — header not reachable at all

Add an explicit dep to the failing target:
`gn deps += [ "//chrome/browser/<subfolder>" ]`

Or, if `//chrome/browser:browser` already depends on `::impl` and the cycle is
intentional, use `allow_circular_includes_from` on `::impl`:
`gn allow_circular_includes_from = [ "//chrome/browser:browser_public_dependencies" ]`

### Error type 2: "Can't include this header from here" — reachable only via private dep

Add the dep directly to the failing target, or promote it to `public_deps` on
the target that owns the header.

### `allow_circular_includes_from` rules

- Set it **on** the target that **owns** the headers you want to expose through
  the cycle.
- The dep cycle must already exist in the dep graph.
- It only covers headers in that target's own `sources`, not headers from its
  transitive deps.
- Standard pattern: `::impl` sets
  `allow_circular_includes_from = ["//chrome/browser:browser_public_dependencies"]`.

## Phase 6: Format, verify, and commit

### Format

Always run after every edit: `bash git cl format`

### Verify the build

```bash
# Desktop Linux (primary — always run this):
python3 tools/mb/mb.py gen -m tryserver.chromium.linux -b linux-rel \
  --config-file tools/mb/mb_config.pyl out/Default

# Android arm64 (run if the subfolder has Android-specific code):
python3 tools/mb/mb.py gen -m tryserver.chromium.android -b android-arm64-rel \
  --config-file tools/mb/mb_config.pyl out/Default
```

Fix any include errors and re-run until clean.

### Commit

Wrap the commit message at 72 characters:

```
Modularize chrome/browser/<subfolder>/BUILD.gn targets

The monolithic `chrome/browser` target leads to long compilation
times and potential circular dependencies. This change extracts
the C++ and header files from `chrome/browser/<subfolder>/` into
dedicated `source_set` targets:

- `:<subfolder>` — header-only interface target
- `:impl` — implementation (.cc files)
- `:unit_tests` — unit tests (moved from chrome/test:unit_tests)
- `:browser_tests` — browser tests (moved from
  chrome/test:browser_tests)

Platform-specific conditions and dependencies were preserved
identically to the original `chrome/browser` configuration.
Missing dependencies implicit to the monolithic target were
explicitly declared.

Bug: <bug-id>
```

## Common pitfalls

- **Existing BUILD.gn ≠ modularized**: Always grep `chrome/browser/BUILD.gn` to
  confirm files have actually been moved out.

- **Over-broad `public_deps`**: Every entry in `public_deps` is transitive — all
  downstream consumers pull it in. Audit each one; if a type is only used in
  `.cc` files, move it to private `deps`.

- **Missing `allow_circular_includes_from`**: If a `.h` in `::impl` is still

- **Missing platform guards**: Copy `if (is_android)`, `if (is_chromeos)`,
  `if (enable_extensions)` verbatim from the original.

- **Unnecessary `BUILDFLAG` guards**: If a class uses a platform-specific
  delegate, prefer passing `nullptr` on other platforms and null-checking at
  runtime instead of build-guarding the whole file.

- **`scoped_mock_*` headers**: Keep in the interface target if used by tests
  outside the folder; move `.cc` to `::impl` or a test-only target.

- **Forgetting `git cl format`**: Always format after changes before verifying
  or committing.
