---
name: angle-branch-merge
description: >-
  Use when merging, cherry-picking, or rebasing an ANGLE CL onto a Chromium
  milestone release branch (e.g., chromium/8127 or M158), or resolving conflicts
  on a chrome-cherry-picker ANGLE merge CL.
---

# Merging an ANGLE CL to a Release Branch

ANGLE release branches live in the standalone `angle/angle` repository as
`refs/heads/chromium/{branch_num}` while the corresponding Chromium milestone
branch lives in `chromium/src` as `refs/branch-heads/{branch_num}`. Performing
and validating an ANGLE merge inside `third_party/angle` of a Chromium release
branch checkout ensures the backport compiles cleanly with `chrome` and passes
`angle_unittests` and `angle_end2end_tests` against the exact milestone
dependencies. Follow the
[Chromium Merge Request Process](https://chromium.googlesource.com/chromium/src.git/+/refs/heads/main/docs/process/merge_request.md#Landing-an-approved-merge)
alongside the ANGLE-specific steps below.

## Prerequisites & Inputs

Before modifying any branches, confirm or gather:

1. **Source CL & Target Branch**:
   - Original `angle/angle` `main` CL number or `Change-Id` (`I...`).
   - Milestone number `M{milestone}` and 4-digit branch number `{branch_num}`
     (for example, M158 -> `8127`, meaning Chromium `branch-heads/8127` and
     ANGLE `chromium/8127`).
2. **Build Directory (`{out_dir}`)**:
   - Confirm which Chromium output directory to use (for example, `out/Release`
     or `out/Default`) before syncing or building. Ask the user if unclear.
3. **Upload State (WIP vs. Ready for Review)**:
   - Confirm whether the uploaded patchset should stay Work-in-Progress (no mail
     sent) or be sent for review (`-r <reviewer> --send-mail`).
4. **`depot_tools` on `PATH`**:
   - Non-interactive subshells often skip `~/.bashrc` and fail with
     `git: 'cl' is not a git command`. Check
     `which gclient git-cl autoninja vpython3` upfront; if missing, locate
     `depot_tools` and prepend it to `PATH` in every command invocation.

## Workflow

### Step 1: Check Gerrit for an Existing Cherry-Pick CL First

When a merge is approved on Chromium Issue Tracker, automation
(`chrome-cherry-picker@chops-service-accounts.iam.gserviceaccount.com`) usually
creates a WIP cherry-pick CL on `angle/angle` branch `chromium/{branch_num}`
sharing the original `Change-Id`. If cherry-picking hit conflicts, Gerrit
creates the WIP CL anyway with literal `<<<<<<<` conflict markers committed in
Patchset 1.

**Always query Gerrit before creating a cherry-pick** so you do not create a
duplicate CL or lose `cherry_pick_of_change` tracking metadata:

```bash
# 1. Inspect the original main CL to get its Change-Id, Bug, and file list:
curl -s "https://chromium-review.googlesource.com/changes/angle%2Fangle~{source_cl}?o=CURRENT_REVISION&o=CURRENT_COMMIT&o=CURRENT_FILES" \
  | tail -n +2

# 2. Search for an existing cherry-pick CL on chromium/{branch_num}:
curl -s "https://chromium-review.googlesource.com/changes/?q=project:angle/angle+branch:chromium/{branch_num}+change:{change_id}&o=CURRENT_REVISION&o=CURRENT_COMMIT" \
  | tail -n +2
```

- **If `{merge_cl}` already exists on `chromium/{branch_num}`**: Reuse it in
  Step 3. ANGLE committers can upload new patchsets directly onto
  `chrome-cherry-picker`-owned CLs.
- **If no CL exists on `chromium/{branch_num}`**: Create one via
  `git cl cherry-pick --host="https://chromium-review.googlesource.com" --branch="refs/heads/chromium/{branch_num}" --allow-conflicts {main_commit_sha}`
  rather than a raw `git cherry-pick`, so Gerrit records `cherry_pick_of_change`
  and creates `{merge_cl}` even when conflicts are present (without blocking on
  an interactive prompt). Fall back to a local
  `git cherry-pick -x {main_commit_sha}` in Step 3 only if `git cl cherry-pick`
  cannot create a server-side CL; if so, prefix the commit title with
  `[M{milestone}] ` and keep the original `Change-Id:`.

### Step 2: Checkout and Sync the Chromium Release Branch Safely

Order matters when switching a Chromium `main` checkout to a release branch:

1. **Check for uncommitted changes and detach `third_party/angle` first**:

   - Inspect `git status -sb` in `src/` and
     `git -C third_party/angle status -sb`. Do not clobber uncommitted work; ask
     or stash first.
   - **Gotcha**: If `third_party/angle` is sitting on a local feature branch
     tracking `origin/main`, `gclient sync` will attempt to rebase that local
     branch onto `chromium/{branch_num}` and fail mid-sync with rebase
     conflicts. Detach `third_party/angle` first so local branches remain
     untouched at their commits:
     ```bash
     git -C third_party/angle checkout --detach
     ```

2. **Fetch `branch-heads/{branch_num}`, sync with `--with_branch_heads`, then
   set `git cl upstream`**:

   - **Gotcha**: Running `git cl upstream branch-heads/{branch_num}` *before*
     `gclient sync --with_branch_heads` fails with
     `starting point 'branch-heads/...' is not a branch` when the checkout has
     not yet configured the `branch-heads` fetch refspec.

   ```bash
   git fetch origin "refs/branch-heads/{branch_num}:refs/remotes/branch-heads/{branch_num}"
   git checkout -B "m{milestone}-{branch_num}" "refs/remotes/branch-heads/{branch_num}"
   gclient sync --with_branch_heads
   git cl upstream "branch-heads/{branch_num}"
   ```

3. **Watch out for moved nested `DEPS` subrepos (avoid `gclient sync -D`)**:

   - **Gotcha**: Omit `-D` when switching between `main` and an older release
     branch. When a nested `DEPS` entry on `main` (for example,
     `third_party/wayland-protocols/kde/src`) replaces a directory that was
     tracked directly inside a parent subrepo on the release branch
     (`third_party/wayland-protocols/kde`), `gclient sync -D` deletes the nested
     directory *after* syncing the parent repo, causing missing-file build
     errors.
   - If `-D` was already used or `git checkout` printed
     `warning: unable to rmdir '...': Directory not empty`, check `git status`
     inside the affected `third_party/` subrepo and restore deleted tracked
     files:
     ```bash
     git -C third_party/<affected_subrepo> reset --hard HEAD
     ```

### Step 3: Patch (or Locally Cherry-Pick) the Merge CL in `third_party/angle` and Fix Upstream

Inside `third_party/angle`, create a clean local branch off
`origin/chromium/{branch_num}`, patch `{merge_cl}`, and **immediately fix the
branch upstream**:

```bash
cd third_party/angle
git fetch origin "refs/heads/chromium/{branch_num}:refs/remotes/origin/chromium/{branch_num}"
git checkout -B "m{milestone}-merge-{merge_cl}" "origin/chromium/{branch_num}"
git cl patch -f "https://chromium-review.googlesource.com/c/angle/angle/+/{merge_cl}"
git cl upstream "origin/chromium/{branch_num}"
```

- **CRITICAL GOTCHA**: `git cl patch` sets
  `branch.<name>.gerritissue = {merge_cl}`, **but resets `branch.<name>.merge`
  to `refs/heads/main`**, even if the branch started from
  `origin/chromium/{branch_num}` or `-b` was passed! Always run
  `git cl upstream "origin/chromium/{branch_num}"` *after* `git cl patch` and
  verify both settings before proceeding:
  ```bash
  git config --get-regexp "branch\.m{milestone}-merge-{merge_cl}\.(merge|gerritissue)"
  # Expected:
  # branch.m{milestone}-merge-{merge_cl}.merge refs/heads/chromium/{branch_num}
  # branch.m{milestone}-merge-{merge_cl}.gerritissue {merge_cl}
  ```
- **Local cherry-pick fallback (if no `{merge_cl}` exists on Gerrit yet)**: Skip
  `git cl patch` and cherry-pick directly onto the local branch off
  `origin/chromium/{branch_num}`:
  ```bash
  cd third_party/angle
  git fetch origin "refs/heads/main:refs/remotes/origin/main" \
    "refs/heads/chromium/{branch_num}:refs/remotes/origin/chromium/{branch_num}"
  git checkout -B "m{milestone}-merge-{source_cl}" "origin/chromium/{branch_num}"
  git cl upstream "origin/chromium/{branch_num}"
  git cherry-pick -x "{main_commit_sha}"
  ```
  In this fallback path, `branch.<name>.gerritissue` will only be populated
  after the first `git cl upload` in Step 6 creates `{merge_cl}`.

### Step 4: Resolve ANGLE-Specific Conflicts and Audit Semantic Drift

Locate all conflict markers (`git grep -n '<<<<<<<'`). Apply ANGLE's
domain-specific rules rather than blindly picking `--ours` or `--theirs`:

1. **`include/GLSLANG/ShaderLang.h` (`ANGLE_SH_VERSION`)**:

   - Never copy `main`'s higher `ANGLE_SH_VERSION` number onto a release branch.
     ANGLE's `_CheckShaderVersionInShaderLangHeader` presubmit requires
     incrementing the **target release branch's** current `ANGLE_SH_VERSION` by
     `+1`:
     ```bash
     git show "origin/chromium/{branch_num}:include/GLSLANG/ShaderLang.h" \
       | grep "define ANGLE_SH_VERSION"
     ```
   - If `ShCompileOptions` bitfields changed, verify bitfield declarations and
     surrounding members against `origin/chromium/{branch_num}`.

2. **ANGLE Features (`include/platform/*_features.json` and `*_autogen.*`)**:

   - Do **not** hand-merge conflict markers in
     `include/platform/autogen/Features*_autogen.h` or
     `util/autogen/angle_features_autogen.{h,cpp}` — manual resolution easily
     retains `main`-only or renamed `Feature::*` enums that do not exist in the
     branch's JSON definitions and breaks compilation.
   - Resolve only the JSON source files (`include/platform/*_features.json`),
     then regenerate and verify using `include/platform/gen_features.py`:
     ```bash
     (
       cd include/platform &&
       vpython3 gen_features.py &&
       vpython3 gen_features.py --verify-only
     )
     ```
   - Note: `gen_features.py` is not listed under
     `scripts/code_generation_hashes/`. If the CL modifies other generated
     files, inspect `scripts/run_code_generation.py` for the corresponding
     generator script.

3. **Semantic C++ Drift Audit**:

   - Even in files without git conflict markers, review every touched function
     against `origin/chromium/{branch_num}` to confirm all helper functions,
     struct fields, renderer feature flags, and enum values referenced by the
     cherry-pick exist with compatible semantics on the release branch.

### Step 5: Build and Run Chrome & ANGLE Test Suites

Return to the Chromium `src/` root, then build `chrome`, `angle_end2end_tests`,
and `angle_unittests`:

```bash
cd ../..
autoninja --quiet -C "{out_dir}" chrome angle_end2end_tests angle_unittests
```

Run both test suites (use `testing/xvfb.py` on headless Linux):

1. **Run `angle_unittests`**:

   ```bash
   testing/xvfb.py "{out_dir}/angle_unittests"
   ```

2. **Run `angle_end2end_tests` (targeted + related suites)**:

   - First run the exact test(s) added or modified by the CL:
     ```bash
     testing/xvfb.py "{out_dir}/angle_end2end_tests" \
       --gtest_filter="*MyAddedOrModifiedTest*"
     ```
   - For broader suites (hundreds or thousands of tests), **always pass
     `--bot-mode` and `--results-file`**:
     ```bash
     testing/xvfb.py "{out_dir}/angle_end2end_tests" \
       --bot-mode \
       --results-file="/tmp/angle_end2end_results.json" \
       --gtest_filter="*RelevantSuite*"
     ```
   - **Why `--bot-mode`**: Running thousands of `Vulkan_SwiftShader` tests in a
     single process without `--bot-mode` can segfault during process teardown in
     `marl::Scheduler::unbind()`, whereas `--bot-mode` runs isolated
     multiprocess batches in parallel like CQ bots.
   - **Why `--results-file`**: Without `--results-file`, `--bot-mode` writes
     `output.json` into the current working directory (`src/`), leaving
     untracked artifacts in the checkout.
   - **Headless Linux OpenGL note**: Under Xvfb's software Mesa `llvmpipe`
     driver, some unrelated `ES3_1_OpenGL` desktop GL tests may fail due to
     software driver limitations; verify your new/relevant tests pass across
     supported backends (`Vulkan_SwiftShader`, `ES3_OpenGL`, etc.) and compare
     against unpatched `origin/chromium/{branch_num}` if in doubt.

### Step 6: Format, Stage Explicitly, Presubmit, and Upload

Inside `third_party/angle`:

1. **Format and stage explicit file paths only**:

   ```bash
   cd third_party/angle
   git cl format
   ```

   - **Gotcha**: Stage **explicit file paths only** (`git add <file1> <file2>`),
     never `git add -u` or `git commit -a`. Inside a Chromium checkout,
     `third_party/angle` has unbootstrapped/dirty nested submodule gitlinks
     (such as `third_party/VK-GL-CTS/src` and `third_party/rapidjson/src`) that
     `-u` or `-a` can accidentally stage into your commit.
   - Amend the patched commit without altering the commit message footers
     (`Change-Id:`, `Bug:`, `(cherry picked from commit ...)`):
     ```bash
     GIT_EDITOR=true git commit --amend --no-edit
     ```
     If you used the local `git cherry-pick -x` fallback and Git is paused at
     `CHERRY_PICK_HEAD`, complete the cherry-pick with `git commit` instead of
     `--amend`, prefixing the commit title with `[M{milestone}] ` and keeping
     the original `Change-Id:`.
   - Verify `git show --stat`, `git diff "origin/chromium/{branch_num}"`, and
     confirm `git grep -n '<<<<<<<'` returns no matches in tracked files.

2. **Handle `PRESUBMIT.py` in an un-bootstrapped `third_party/angle`**:

   - Run `git cl presubmit --upload`.
   - In a standard Chromium checkout (where `third_party/angle` has not been
     standalone-bootstrapped via `scripts/bootstrap.py`), ANGLE's `PRESUBMIT.py`
     fails on two standalone-only checks that expect standalone
     `third_party/angle/third_party/` checkouts:
     - `scripts/export_targets.py` (`third_party/depot_tools/gn.py` missing)
     - `scripts/run_code_generation.py --verify-no-dirty`
       (`third_party/spirv-headers` / `vulkan-headers` missing)
   - Verify that **every other presubmit check passes** (formatting, commit
     footers, `_CheckShaderVersionInShaderLangHeader`, and
     `vpython3 gen_features.py --verify-only`), report the two standalone-only
     presubmit failures clearly, and **ask the user for confirmation** before
     passing `--bypass-hooks`.

3. **Upload the resolved patchset to Gerrit**:

   - Re-verify `branch.<name>.merge` is `refs/heads/chromium/{branch_num}` and
     `branch.<name>.gerritissue` is `{merge_cl}` (for the local
     `git cherry-pick -x` fallback, `gerritissue` is unset before upload and is
     populated once this first `git cl upload` creates `{merge_cl}`).
   - To keep the CL **WIP** (no email notifications):
     ```bash
     # Updating an existing {merge_cl}:
     git cl upload -f --title="Resolve merge conflicts"
     # Or initial upload from local cherry-pick fallback (omit --title):
     git cl upload -f
     ```
     (append `--bypass-hooks` only if approved by the user in step 2).
   - To mark **Ready for Review**, pass `-r <reviewer> --send-mail` (typically
     the reviewer of the original `main` CL).
   - Query Gerrit afterward to confirm the new patchset landed on `{merge_cl}`
     targeting `chromium/{branch_num}` with the expected parent commit and
     changed files:
     ```bash
     curl -s "https://chromium-review.googlesource.com/changes/angle%2Fangle~{merge_cl}?o=CURRENT_REVISION&o=CURRENT_COMMIT&o=CURRENT_FILES" \
       | tail -n +2
     ```

### Step 7 (Optional): Return the Checkout to `origin/main`

If switching the Chromium workspace back to `origin/main` after uploading, first
return from `third_party/angle` to the Chromium `src/` root:

1. Return to the Chromium `src/` root and detach `third_party/angle` first so
   `gclient sync` does not try to rebase `m{milestone}-merge-{merge_cl}` onto
   `origin/main`:
   ```bash
   cd ../..
   git -C third_party/angle checkout --detach
   ```
2. Fetch `origin/main` and switch to an existing local branch or create/reset a
   branch off `origin/main`:
   ```bash
   git fetch origin main
   # Switch back to an existing local branch:
   git checkout <branch_name>
   # Or create a new branch off origin/main (use -B to reset an existing branch):
   git checkout -b <branch_name> origin/main
   ```
   - **Gotcha**: If a `third_party/<subrepo>` directory was a separate `DEPS`
     git repo on the release branch (for example, `third_party/libunwindstack`)
     but is tracked directly inside `chromium/src` on `main`, `git checkout`
     aborts with
     `The following untracked working tree files would be overwritten`. Verify
     `git -C third_party/<subrepo> status --porcelain` is empty, remove that
     directory, and re-run `git checkout`.
3. From the Chromium `src/` root, run `gclient sync` to re-align all
   subrepositories and hooks with `main`.
