---
name: enable-check-includes-strict
description: >-
  Enable and migrate Chromium targets and directories to check_includes_strict =
  true, diagnosing and resolving header include violations as part of the
  check_includes_strict LSC. Use when adding directories to the allowlist,
  enabling strict header checks on targets, or resolving
  CHECK_INCLUDES_STRICT_UNTRIAGED targets.
---

# Enable `check_includes_strict`

This skill guides the process of enabling `check_includes_strict = true` across
Chromium targets and directories, following the
[Enable check_includes_strict by default LSC](https://docs.google.com/document/d/1zYUC_zuFrZozo9ogFHcf0Rzkiw9W7RVnaY-1fzfQ0AM/edit?tab=t.0)
([go/check-includes-strict-lsc](http://go/check-includes-strict-lsc), tracking
bug: [crbug.com/555455099](https://crbug.com/555455099)).

`check_includes_strict` is a GN target configuration that prevents including
headers declared in transitive dependencies unless they are forwarded through
explicit `public_deps`.

Note that all VCS operations have been written with `jj`, but that if `jj` is
unavailable, you may instead run equivalent `git` commands.

## Commit message

The following will be added at the end of all commit messages created for
check_includes_strict.

```text
This is part of the check_includes_strict LSC (https://docs.google.com/document/d/1zYUC_zuFrZozo9ogFHcf0Rzkiw9W7RVnaY-1fzfQ0AM/edit?usp=sharing)

Bug: 555455099
```

## Preparing output directories

If the output directories do not yet exist, you will need to prepare them. The
recommended output directories are:

```sh
python3 tools/mb/mb.py gen -m tryserver.chromium.linux -b linux-libfuzzer-asan-rel //out/linux-libfuzzer-asan-rel
python3 tools/mb/mb.py gen -m tryserver.chromium.mac -b mac-rel //out/mac-rel
python3 tools/mb/mb.py gen -m tryserver.chromium.win -b win_chromium_compile_rel_ng //out/win_chromium_compile_rel_ng
python3 tools/mb/mb.py gen -m tryserver.chromium.android -b android-cronet-arm-rel //out/android-cronet-arm-rel
python3 tools/mb/mb.py gen -m tryserver.chromium.fuchsia -b fuchsia-x64-cast-receiver-rel //out/fuchsia-x64-cast-receiver-rel
python3 tools/mb/mb.py gen -m tryserver.chromium.chromiumos -b linux-chromeos-rel //out/linux-chromeos-rel
```

Also add `target_os = ["android", "chromeos", "fuchsia", "mac", "win"]` and
`"custom_vars": { "checkout_src_internal": True }` to your `.gclient` and run
`gclient sync`.

To verify that they work, run `gn gen` on each of these output directories until
they are all passing.

## Onboarding new directories

When onboarding a new directory (e.g., `//base/*`):

1. **Start working**

   - `jj new $BASE_COMMIT` (`$BASE_COMMIT` is probably `main@origin`).

2. **Create empty commit**

   - `jj commit -m $COMMIT_MSG` (yes, this commit is intentionally empty, we'll
     squash into it later)
   - The message's first line should be
     `Fix violations in making //base/* use check_includes_strict`

3. **Add Directory to Allowlist**:

   - Edit [build/strict_deps_failures.gni](//build/strict_deps_failures.gni) and
     add the directory path to `CHECK_INCLUDES_STRICT_ALLOWLIST`
   - `jj commit -m $COMMIT_MSG`
   - The message's first line will be
     `Enable check_includes_strict for //base/*`

4. **Fix failures**:

   - Run `gn check --fix --error-limit=-1 $OUT_DIR` on each output directory
   - Run in the order specified above in the "preparing output directories"
     section.
   - Run it iteratively until there are no failures for that output directory,
     then move on to the next output directory.
     - It may produce errors that require manual intervention or decisions to be
       made. If there is an obviously correct way to do it, do it. Otherwise,
       ask what to do. While you are waiting for an answer, continue working.
     - Note that if gn suggests to put it behind a condition, we should *ALWAYS*
       do so.
     - For simple conditions, either add it to an existing block with
       `deps/public_deps` in it, or create a new block with either `deps` or
       `public_deps` as required.
     - For more complex conditions, you are allowed to merge it into the
       existing conditional where the sources are added. You may need to add
       `deps = []` above the conditional.
     - Do not inspect `.cc` files for include errors: GN's suggestion output
       already gives you the destination target, the exact condition, and the
       dependency to add.
     - Run `jj diff` and:
     - Find any TODOs added and resolve them
     - Perform a once-over to verify that the changes look reasonable
     - Downstream churn across the repo is expected. For example, when enabling
       `check_includes_strict` on `//base`, downstream targets across the entire
       repo may find that they need to explicitly declare a dependency on
       `//third_party/abseil-cpp:absl`, which was previously implicitly allowed
       through `//base`.
     - Run `jj squash --to @--`
   - Once it passes for all output directories, run it once more for each output
     directory without `--fix` to verify that the fixes for the later output
     directories did not break the former ones.

5. **Upload change**

   - Run `jj fix`
   - Run `jj gerrit upload --label=Owners-Override --label=Commit-Queue+1`
