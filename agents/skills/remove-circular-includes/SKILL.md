---
name: remove-circular-includes
description: >-
  Systematically eliminate allow_circular_includes_from declarations in Chromium
  GN build files while preserving build graph equivalence and ensuring
  multi-platform build and header-check correctness.
---

# Eliminating `allow_circular_includes_from` in Chromium

Guide for safely removing `allow_circular_includes_from` declarations across the
Chromium codebase, following the [Remove Circular Includes LSC][lsc-doc]
([go/remove-circular-includes-lsc](http://go/remove-circular-includes-lsc),
tracking bug: [crbug.com/556482973](https://crbug.com/556482973)).

Note that all VCS operations have been written with `jj`, but that if `jj` is
unavailable, you may instead run equivalent `git` commands.

## Preparing output directories

If the output directories do not yet exist, you will need to prepare them. The
recommended output directories are:

```sh
python3 tools/mb/mb.py gen -m tryserver.chromium.linux \
  -b linux-libfuzzer-asan-rel //out/linux-libfuzzer-asan-rel
python3 tools/mb/mb.py gen -m tryserver.chromium.mac -b mac-rel //out/mac-rel
python3 tools/mb/mb.py gen -m tryserver.chromium.win \
  -b win_chromium_compile_rel_ng //out/win_chromium_compile_rel_ng
python3 tools/mb/mb.py gen -m tryserver.chromium.android \
  -b android-cronet-arm-rel //out/android-cronet-arm-rel
python3 tools/mb/mb.py gen -m tryserver.chromium.fuchsia \
  -b fuchsia-x64-cast-receiver-rel //out/fuchsia-x64-cast-receiver-rel
python3 tools/mb/mb.py gen -m tryserver.chromium.chromiumos \
  -b linux-chromeos-rel //out/linux-chromeos-rel
```

Also add `target_os = ["android", "chromeos", "fuchsia", "mac", "win"]` and
`"custom_vars": { "checkout_src_internal": True }` to your `.gclient` and run
`gclient sync`.

To verify that they work, run `gn gen` on each of these output directories until
they are all passing.

## Diagnosis and fixing

Before fixing any targets, run `gn gen` and
`siso query graph -C $OUT_DIR > $OUT_DIR/baseline.jsonl`. If the build file in
question is specific to particular platforms, you should do this for each
different output directory. Otherwise, `linux-libfuzzer-asan-rel` will suffice.

In GN C/C++ targets, a target `A` should depend on a target `B` iff `A`'s
sources `#include` `B`'s public (or sources, if friends is used). This will
henceforth be referred to as "`A` includes `B`".

`allow_circular_includes_from` allows you to break this rule and not depend on
`B`. When `A` declares `allow_circular_includes_from = [ B ]`, there are several
cases. The following pseudocode should be taken to fix it:

```
remove the allow_circular_includes_from entry
if A does not transitively depend on B or B does not include A
  # allow_circular_includes_from is obsolete
  run `gn check --fix`
elif A includes B
  # This is a true circular dependency.
  Either break it at the source-file level (eg. by extracting code into a
  common header and declaring that as its own source set), or both headers
  should be part of the same target.
elif A is a link target (eg. shared_library / static_library) with
     public / sources
  Copy the block for A's definition into a new block `source_set("A_sources")`
  Remove public / sources from A
  Add A_sources to the public_deps of A
  Remove deps / public_deps from A_sources
  run `gn check --fix` to add the deps to A_sources and A_sources to B
  Once it's fixed, rerun `gn gen` and
  `siso query graph ... > $OUT_DIR/modified.jsonl`.
  You should expect no changes, except `obj/.../A/foo.o` being renamed to
  `A_sources`.
else
  Run `gn check --fix` to make B depend on A
  Run `gn gen` and see what dependency loop pops up
  Find the entry in the loop without a corresponding header include and
  remove it
  Once it's fixed, rerun `gn gen` and
  `siso query graph ... > $OUT_DIR/modified.jsonl`.
  You should expect no changes.
```

When you run `gn check --fix`, it may complain (if, for example, the correct
target for B to depend is not visible to it). You will need to fix those errors.
Use your best judgement following standard chromium best practices.

## Commit message

The following will be added at the end of all commit messages created for
removing circular includes:

```text
Remove allow_circular_includes_from from //path/to:target

This previously existed because ...

To remove it, we ...

Part of LSC: https://docs.google.com/document/d/1LuFqCJwtzZHuLfoj9yuupZnlCqwnbDXBgmF70tCwxDE/edit?tab=t.0

Bug: 556482973
```

## Core Principles

The most important goal is ensuring reviewers have high confidence in the
correctness of the changes:

- **Readable**: Keep commits modular and cohesive. Simple fixes can be squashed
  together, while complex fixes should have dedicated commits.
- **Simple & Minimal**: Do not perform unrelated changes or refactoring.
- **No Hacks**: Every fix must be clean and idiomatic. Never use tactical hacks
  or suppressions just to make `gn check` pass locally.
- **Banned Anti-Patterns**:
  - **NEVER use `-=`**: Never conditionally subtract sources from targets.
    Target sources must remain explicit, predictable, and traceable.
  - **NEVER have duplicate file declarations**: Never declare the same source or
    header file in multiple targets within any given build graph.
  - **AVOID Architecture-Specific Header Sets**: Helper targets or header sets
    should be architecturally symmetrical across all platforms unless
    platform-specific build flags or headers make a split inherently
    platform-bound.

## Workflow

When resolving an `allow_circular_includes_from` usage:

1. **Create an empty commit**

   - `jj new $baseline`

2. **Diagnose and implement the fix**:

   - Apply the rules defined above to fix the code.

3. **Remove target from allowlist**:

   - Remove the corresponding entry from
     [`build/circular_includes_untriaged.gni`][untriaged] (or
     [`build/circular_includes_bedrock.gni`][bedrock]).

4. **Make the commit**:

   - `jj fix` && `jj describe -m ...`

5. **Potentially squash**

   - If this commit is small, it may be squashed with other small commits. The
     commit messages should be manually combined.

6. **Upload**

   - `jj gerrit upload -r ... --label=Commit-Queue+1`

[bedrock]: //build/circular_includes_bedrock.gni
[lsc-doc]: https://docs.google.com/document/d/1LuFqCJwtzZHuLfoj9yuupZnlCqwnbDXBgmF70tCwxDE/edit?tab=t.0
[untriaged]: //build/circular_includes_untriaged.gni
