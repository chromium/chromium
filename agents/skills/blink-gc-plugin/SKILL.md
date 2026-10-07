---
name: blink-gc-plugin
description: >-
  Develop, build, test, and roll changes to the Blink GC Clang plugin
  (//tools/clang/blink_gc_plugin). Use when modifying or adding Oilpan/Blink GC
  Clang plugin checks, running plugin unit tests, checking Chromium/Blink
  targets for new plugin violations, suppressing violations with
  GC_PLUGIN_IGNORE, or force-rolling Clang for plugin updates.
---

# Blink GC Clang Plugin Development

This skill covers the end-to-end workflow for modifying the Blink GC (Oilpan)
Clang plugin in `//tools/clang/blink_gc_plugin/`, testing it locally, fixing or
suppressing violations across the codebase, rolling out the changes, and
cleaning up the local environment afterwards.

## 1. Key Files and Architecture

- **Plugin Entry Point & AST Consumer:**
  - [BlinkGCPlugin.cpp](//tools/clang/blink_gc_plugin/BlinkGCPlugin.cpp) and
    [BlinkGCPluginOptions.h](//tools/clang/blink_gc_plugin/BlinkGCPluginOptions.h):
    Plugin registration, command-line option definitions, and argument parsing.
  - [BlinkGCPluginConsumer.h](//tools/clang/blink_gc_plugin/BlinkGCPluginConsumer.h)
    and
    [BlinkGCPluginConsumer.cpp](//tools/clang/blink_gc_plugin/BlinkGCPluginConsumer.cpp):
    Main `ASTConsumer` that orchestrates declaration collection, record/method
    validation, visitor checks, and AST matcher checks.
  - [CollectVisitor.h](//tools/clang/blink_gc_plugin/CollectVisitor.h) and
    [CollectVisitor.cpp](//tools/clang/blink_gc_plugin/CollectVisitor.cpp):
    `RecursiveASTVisitor` that collects record declarations and tracing method
    definitions in the translation unit.
- **Record & Points-To Edge Model:**
  - [RecordInfo.h](//tools/clang/blink_gc_plugin/RecordInfo.h) and
    [RecordInfo.cpp](//tools/clang/blink_gc_plugin/RecordInfo.cpp): Caches
    record properties (whether a type is garbage-collected, traceable,
    stack-allocated, or has fields/bases requiring tracing).
  - [Edge.h](//tools/clang/blink_gc_plugin/Edge.h),
    [Edge.cpp](//tools/clang/blink_gc_plugin/Edge.cpp),
    [NeedsTracing.h](//tools/clang/blink_gc_plugin/NeedsTracing.h), and
    [TracingStatus.h](//tools/clang/blink_gc_plugin/TracingStatus.h): Models the
    type/field edge graph (`Value`, `RawPtr`, `RefPtr`, `UniquePtr`, `Member`,
    `WeakMember`, `Persistent`, `CrossThreadPersistent`, `Collection`,
    `Iterator`, `ArrayEdge`, etc.), `EdgeVisitor` / `RecursiveEdgeVisitor`, and
    tracing requirements.
- **Visitor-Based Checks:**
  - [CheckFieldsVisitor.h](//tools/clang/blink_gc_plugin/CheckFieldsVisitor.h)
    and
    [CheckFieldsVisitor.cpp](//tools/clang/blink_gc_plugin/CheckFieldsVisitor.cpp):
    `RecursiveEdgeVisitor` that validates class fields (e.g. disallowing raw,
    `scoped_refptr`, or `unique_ptr` pointers to GC-managed/traceable types,
    disallowing `Member` in unmanaged classes, disallowing pointers from heap
    types to stack-allocated types, and disallowing GC-derived part objects).
  - [CheckForbiddenFieldsVisitor.h](//tools/clang/blink_gc_plugin/CheckForbiddenFieldsVisitor.h)
    and
    [CheckForbiddenFieldsVisitor.cpp](//tools/clang/blink_gc_plugin/CheckForbiddenFieldsVisitor.cpp):
    `RecursiveEdgeVisitor` that checks that GC-managed classes and their
    embedded objects do not contain forbidden fields (such as `TaskRunner` or
    Mojo `Remote`/`Receiver` types).
  - [CheckGCRootsVisitor.h](//tools/clang/blink_gc_plugin/CheckGCRootsVisitor.h)
    and
    [CheckGCRootsVisitor.cpp](//tools/clang/blink_gc_plugin/CheckGCRootsVisitor.cpp):
    `RecursiveEdgeVisitor` that checks that on-heap classes and their part
    objects do not contain GC roots (`Persistent`).
  - [CheckTraceVisitor.h](//tools/clang/blink_gc_plugin/CheckTraceVisitor.h) and
    [CheckTraceVisitor.cpp](//tools/clang/blink_gc_plugin/CheckTraceVisitor.cpp):
    `RecursiveASTVisitor` that inspects `Trace` method bodies to verify all
    required bases and fields are traced.
  - [CheckDispatchVisitor.h](//tools/clang/blink_gc_plugin/CheckDispatchVisitor.h)
    and
    [CheckDispatchVisitor.cpp](//tools/clang/blink_gc_plugin/CheckDispatchVisitor.cpp):
    `RecursiveASTVisitor` that verifies manual dispatch in `TraceAfterDispatch`
    and `FinalizeGarbageCollectedObject` methods.
  - [CheckFinalizerVisitor.h](//tools/clang/blink_gc_plugin/CheckFinalizerVisitor.h)
    and
    [CheckFinalizerVisitor.cpp](//tools/clang/blink_gc_plugin/CheckFinalizerVisitor.cpp):
    `RecursiveASTVisitor` that checks destructors/finalizers do not access
    potentially finalized fields.
- **AST Matcher-Based Checks:**
  - [BadPatternFinder.h](//tools/clang/blink_gc_plugin/BadPatternFinder.h) and
    [BadPatternFinder.cpp](//tools/clang/blink_gc_plugin/BadPatternFinder.cpp):
    Clang AST matchers for disallowed patterns involving Oilpan types (such as
    `std::optional`, `absl::optional`, `std::variant`, `absl::variant`,
    `base::raw_ptr`, `base::raw_ref`, `std::unique_ptr`, `base::WeakPtr`,
    off-heap collections, `Member` on stack, GCed variables/fields by value, and
    extra padding).
- **Configuration, Diagnostics, Graph Dumping, and Tests:**
  - [Config.h](//tools/clang/blink_gc_plugin/Config.h) and
    [Config.cpp](//tools/clang/blink_gc_plugin/Config.cpp): Helpers for
    identifying Oilpan types, annotations (`STACK_ALLOCATED`,
    `GC_PLUGIN_IGNORE`), and smart pointer/collection names.
  - [DiagnosticsReporter.h](//tools/clang/blink_gc_plugin/DiagnosticsReporter.h)
    and
    [DiagnosticsReporter.cpp](//tools/clang/blink_gc_plugin/DiagnosticsReporter.cpp):
    Diagnostic message templates and reporting methods.
  - [JsonWriter.h](//tools/clang/blink_gc_plugin/JsonWriter.h) and
    [process-graph.py](//tools/clang/blink_gc_plugin/process-graph.py):
    Utilities for dumping (`dump-graph`) and processing the object graph.
  - `//tools/clang/blink_gc_plugin/tests/`: Lit-style golden tests (`.cpp`,
    `.h`, optional `.flags`, and expected `.txt` output files) run via
    [test.py](//tools/clang/blink_gc_plugin/tests/test.py).

> **Note on Conservative Stack Scanning (CSS):** Oilpan uses CSS when scanning
> the stack. References to GCed objects and into GCed objects (such as traceable
> objects or `Member`s) are generally safe on the stack (in local variables or
> fields of `STACK_ALLOCATED()` classes) because CSS will find the pointers on
> the stack and retain the relevant/containing objects. (Allocating or holding a
> GCed object *by value* on the stack remains disallowed.)

## 2. Building Clang and Running Plugin Unit Tests

1. **Initial Clang build** (run once from `chromium/src` root):

   ```bash
   ./tools/clang/scripts/build.py --without-android --without-fuchsia
   ```

2. **Incremental Clang rebuilds** (use for all subsequent edits to the plugin):

   ```bash
   ninja -C third_party/llvm-build/Release+Asserts/ clang
   ```

   > **Troubleshooting:** If `ninja` re-runs CMake and fails because
   > `tensorflow/xla_aot_runtime_src` is missing from the `vpython` cache,
   > restore the environment by running
   > `vpython3 tools/clang/scripts/get_tensorflow.py` and then re-run `ninja`.

3. **Run the plugin unit tests**:

   ```bash
   (cd tools/clang/blink_gc_plugin/tests && ./test.py ../../../../third_party/llvm-build/Release+Asserts/bin/clang)
   ```

4. **Update or add unit tests**:

   - Extend existing tests or add new `.cpp`/`.h`/`.txt` test cases in
     `//tools/clang/blink_gc_plugin/tests/` to cover all newly checked types and
     edge cases (heap vs. stack, raw pointers/references, `new` expressions).
   - If a test requires a plugin flag that is disabled by default, add a
     `<test_name>.flags` file in `//tools/clang/blink_gc_plugin/tests/` with the
     required compiler flags (e.g.
     `-Xclang -plugin-arg-blink-gc-plugin -Xclang <new-flag>`; see
     [extra_padding.flags](//tools/clang/blink_gc_plugin/tests/extra_padding.flags)
     for an example).
   - Be careful when running `git cl format`: formatting test `.cpp`/`.h` files
     may change line numbers expected by the `.txt` golden files.

## 3. Finding and Suppressing Codebase Violations

1. **Configure a debug build directory**: Use `is_debug = true` to maximize code
   coverage across debug-only assertions and declarations:

   ```bash
   gn gen out/gc_plugin_check --args="is_debug = true use_remoteexec = true"
   ```

   *(Note: If compiling directly with the locally built `clang` binary, RBE
   cannot execute your local compiler remotely, so you may need
   `use_remoteexec = false` during local plugin sweeps.)*

2. **Build only targets affected by `blink-gc-plugin` (if space/time is
   tight)**:

   - Building `all` targets can consume hundreds of gigabytes of disk space
     during linking.
   - Only three GN configs enable `blink-gc-plugin`:
     - `//third_party/blink/renderer:config` (note: `all_blink` covers most
       Blink targets, but some standalone test targets like
       `blink_heap_perftests` depend on renderer configs without being in
       `all_blink`)
     - `//v8:internal_config_base`
     - `//third_party/pdfium:pdfium_common_config`
   - You can find all targets that use `blink-gc-plugin` by querying
     `build.ninja` for targets with `blink-gc-plugin` in their flags, and build
     their `.stamp` / object targets with
     `autoninja -C out/gc_plugin_check -k 0` to find all compile-time plugin
     errors without linking huge binaries.
   - If you run out of disk space and cannot build everything, prioritize
     `all_blink` or `chrome` and inform the user.
   - There is no need to run Chrome or Blink unit tests when only updating
     static plugin checks and adding `GC_PLUGIN_IGNORE` suppressions.

3. **Fix or suppress existing violations (`GC_PLUGIN_IGNORE`)**:

   - **Prefer landing fixes and `GC_PLUGIN_IGNORE` suppressions in a separate CL
     (or CLs) before the plugin change itself.** This makes reviews much easier
     and keeps the codebase in a clean, safe state even if a Clang roll or the
     plugin CL needs to be reverted.

   - Before adding `GC_PLUGIN_IGNORE` annotations, **ask the user if they have a
     tracking bug (`crbug.com/...`) to reference** (let the user proceed without
     one if they prefer).

   - Use a consistent suppression message across all occurrences so they are
     easy to locate later. Prefix the message with `crbug.com/<bug_id>: ` when a
     tracking bug is provided:

     ```cpp
     #include "third_party/blink/renderer/platform/wtf/gc_plugin.h"

     GC_PLUGIN_IGNORE("crbug.com/123456789: <consistent reason>")
     ```

## 4. Rolling Out Plugin Changes

- **Relaxing a check:** If a plugin change only *relaxes* a check (so previously
  prohibited patterns become allowed), it cannot cause new build failures and is
  **safe to land directly** in a normal CL without gating behind a flag or
  forcing a Clang roll.
- **Adding or tightening a check:** When adding or tightening checks in
  `blink-gc-plugin`, you must **either** gate the new check behind a plugin flag
  **or** force-roll a new Clang package in the same CL. Otherwise, CLs that land
  between your plugin change and the next Clang roll may introduce new
  violations and break the Clang roll.

### Option A: Gating Changes Behind a Plugin Flag

This approach avoids a manual Clang roll, though the full rollout across two
regular Clang rolls typically takes a couple of weeks:

1. **Add the flag (defaulting to `false`)**:
   - Add a boolean field initialized to `false` in `BlinkGCPluginOptions`
     ([BlinkGCPluginOptions.h](//tools/clang/blink_gc_plugin/BlinkGCPluginOptions.h)).
   - Parse the flag string in `BlinkGCPluginAction::ParseArgs`
     ([BlinkGCPlugin.cpp](//tools/clang/blink_gc_plugin/BlinkGCPlugin.cpp)).
   - Gate the new plugin check on this option.
2. **Add unit tests with a `.flags` file**:
   - Add or update tests in `//tools/clang/blink_gc_plugin/tests/` and create a
     corresponding `<test_name>.flags` file containing
     `-Xclang -plugin-arg-blink-gc-plugin -Xclang <new-flag>` (see
     [extra_padding.flags](//tools/clang/blink_gc_plugin/tests/extra_padding.flags))
     so the test runs with the flag enabled even while it is off by default.
3. **Land the plugin CL and wait for a Clang roll**:
   - Land the plugin and test changes. Wait for the next regular Clang roll to
     bring the flag into Chromium's prebuilt Clang.
4. **Enable the flag locally in `BUILD.gn` and land fixes/suppressions**:
   - Locally add
     `"-Xclang", "-plugin-arg-blink-gc-plugin", "-Xclang", "<new-flag>"` to
     `cflags` in
     [third_party/blink/renderer/BUILD.gn](//third_party/blink/renderer/BUILD.gn)
     (see the `blink_gc_plugin_option_do_dump_graph` block in that file for an
     example).
   - Build to find and fix or `GC_PLUGIN_IGNORE` all violations, and land those
     fixes/suppressions in separate CL(s).
5. **Enable the check in `BUILD.gn` and flip the plugin default**:
   - Land the
     [third_party/blink/renderer/BUILD.gn](//third_party/blink/renderer/BUILD.gn)
     change that passes the flag, change the flag's default to `true` in
     [BlinkGCPluginOptions.h](//tools/clang/blink_gc_plugin/BlinkGCPluginOptions.h),
     and remove the `*.flags` files that were added for that flag in
     `//tools/clang/blink_gc_plugin/tests/`.
6. **Clean up the flag after the next Clang roll**:
   - Once another Clang roll lands with the flag enabled by default, remove the
     extra `cflags` from
     [third_party/blink/renderer/BUILD.gn](//third_party/blink/renderer/BUILD.gn)
     and remove the flag from the plugin.

### Option B: Force-Rolling Clang in the Same CL

When force-rolling Clang in the same CL instead of adding a flag (ideally after
landing codebase fixes/suppressions in preceding CLs):

1. **Check for conflicting Clang rolls on Gerrit**:

   - Search Gerrit for active CLs touching
     [update.py](//tools/clang/scripts/update.py).
   - Note that automated Clang+Rust roll CLs (whose subjects start with
     `"Roll clang+rust "`) are generated every few hours and the vast majority
     never land. **Ignore `"Roll clang+rust "` CLs unless they show signs of
     human attention (such as `+1` votes or human review comments), which
     indicate the CL is an active candidate for landing.**
   - **Warn the user** if there is an active Clang roll with human attention or
     another active CL incrementing `CLANG_SUB_REVISION` that they may want to
     wait for, but give the user the option to override and proceed.
   - If another active CL already uses `CLANG_SUB_REVISION + 1`, pick a
     different unused sub-revision number to avoid package collisions.
   - **Remind the user** to reach out to the **Lexan team** (via their chat
     room) to check for ongoing rolls and coordinate the review.

2. **Increment `CLANG_SUB_REVISION`**:

   - Increment `CLANG_SUB_REVISION` in
     [update.py](//tools/clang/scripts/update.py).

3. **Upload the CL and build the Clang packages**:

   - Upload the CL (`git cl upload`) and trigger the 4 Clang upload trybots:
     - `linux_upload_clang`
     - `mac_upload_clang`
     - `mac_upload_clang_arm`
     - `win_upload_clang`
   - **Do NOT run the CQ yet.** Running the CQ before the new packages are
     published and synced in `DEPS` will fail because the bots cannot find the
     new Clang revision.

4. **Publish the packages and wait**:

   - Once all 4 `*_upload_clang` trybots succeed, ask the **Lexan team** to
     publish the new Clang packages.
   - Wait **30–60 minutes** after publication for package propagation.

5. **Update `DEPS` with `sync_deps.py`**:

   - Run [sync_deps.py](//tools/clang/scripts/sync_deps.py) to download the
     published packages, compute their SHA-256 hashes, and update `DEPS`:

     ```bash
     ./tools/clang/scripts/sync_deps.py
     ```

   - Amend the updated `DEPS` file into your commit and upload a new patchset to
     the CL.

6. **Send for review and CQ**:

   - Once approved and `DEPS` is updated, the CL can be submitted via the CQ.

## 5. Post-Development Cleanup

Building Clang locally overwrites `third_party/llvm-build/Release+Asserts/` and
creates `third_party/llvm-build/force_head_revision`. Always clean up when
finished:

1. Remove any temporary Chromium build directories (e.g. `out/gc_plugin_check`).

2. Remove `third_party/llvm-build/Release+Asserts/` and
   `third_party/llvm-build/force_head_revision`.

3. Switch to `main` (or a branch with an already-published `CLANG_SUB_REVISION`
   so the `configure_reclient_cfgs` hook succeeds) and run `gclient sync` to
   redownload the default prebuilt Clang package:

   ```bash
   rm -rf third_party/llvm-build/Release+Asserts/ third_party/llvm-build/force_head_revision
   git checkout main
   gclient sync
   ```
