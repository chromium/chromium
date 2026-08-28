---
name: bisect
description: >-
  Bisect regressions or broken builds in the Chromium repository using deterministic
  binary search, checking out midpoint commits, syncing dependencies with gclient sync,
  compiling the target with autoninja, and querying the user for test results until the
  culprit commit is isolated.
---

# Chromium Bisect

This skill guides the agent through bisecting a regression or broken build in
the Chromium repository using a deterministic binary search across git commits.

## 1. Parameters & Initialization

When bisecting, the following information is needed:

1. **Good Commit**: The known working git commit hash.
2. **Bad Commit**: The known broken git commit hash.
3. **Build Directory**: The output build directory (e.g., `out/ALDebug`,
   `out/Debug`, `out/Default`).
4. **Build Target**: The target to compile (e.g., `chrome_apk`, `chrome`,
   `content_shell_test_apk`, `unit_tests`).

### Invocation Patterns

The skill can be invoked with a commit range and optional build settings, such
as:

```text
/bisect <good_commit>..<bad_commit> , <out_dir> , <build_target>
```

Examples:

- `/bisect hash1..hash2 , out/Debug , content_shell_test_apk`
- "Bisect between commit A and commit B using out/Default chrome"

### Clarifying Missing Parameters

If any required parameters are not provided in the user's prompt:

- **Missing commits**: Ask the user for the known good and bad commit hashes.
- **Missing build directory**: Ask the user which output directory to use (e.g.,
  `out/ALDebug`, `out/Debug`, or `out/Default`).
- **Missing build target**: Ask the user which build target to compile (e.g.,
  `chrome_apk`, `chrome`).

______________________________________________________________________

## 2. Workspace Pre-Check (Uncommitted Changes)

Before checking out commits or beginning the bisect loop, verify that the
workspace has no uncommitted modifications to tracked files, as local changes
can block `git checkout` or cause merge conflicts.

Run:

```bash
git status --porcelain -uno
```

- If output is **empty**: The working tree is clean of tracked changes. Proceed
  with the bisect.
- If output is **non-empty**: Inform the user that uncommitted changes to
  tracked files are present and ask them to commit (`git commit`), stash
  (`git stash`), or revert (`git restore`) them before proceeding.

______________________________________________________________________

## 3. Deterministic Midpoint Calculation

To ensure fast and deterministic midpoint selection across the linear git
history, use `git rev-list --reverse` rather than full graph traversal:

```bash
COMMITS=$(git rev-list --reverse <GOOD_COMMIT>..<BAD_COMMIT>)
TOTAL=$(echo "$COMMITS" | wc -l)
MID=$(( (TOTAL + 1) / 2 ))
MID_COMMIT=$(echo "$COMMITS" | sed -n "${MID}p")
echo "Total commits in range: $TOTAL"
echo "Midpoint index: $MID"
echo "Midpoint commit: $MID_COMMIT"
git log -1 --oneline "$MID_COMMIT"
```

______________________________________________________________________

## 4. Step-by-Step Bisection Loop

Execute the following steps for each iteration until the regression is isolated
to a single commit:

### Step 1: Checkout the Midpoint Commit

Check out the computed midpoint commit hash:

```bash
git checkout <MID_COMMIT> && git log -1 --oneline
```

### Step 2: Sync Dependencies & Hooks

Run `gclient sync -DR` to align submodules, third-party dependencies,
toolchains, DEPS, and build hooks with the checked-out commit:

```bash
gclient sync -DR
```

### Step 3: Compile the Build Target

Build the specified target in the target output directory using `autoninja`:

```bash
autoninja -C <out_dir> <target>
```

*Note: If the build runs in the background, monitor its status or use scheduled
timers until completion.*

### Step 4: Query User for Test Result

Once the build completes successfully, present the user with an interactive
question to verify the build on their test setup or device:

- Use the `ask_question` tool with options:
  1. `The build is Good (working)`
  2. `The build is Bad (broken)`
  3. `Skip commit (unbuildable / untestable)`

### Step 5: Update Range & Repeat

Based on the user's response:

- **Good (working)**: Set the new **Good** commit to `<MID_COMMIT>`.
- **Bad (broken)**: Set the new **Bad** commit to `<MID_COMMIT>`.
- **Skip (unbuildable)**: Pick an adjacent commit (e.g., `MID + 1` or `MID - 1`)
  within the range and retest while keeping the outer bounds.

Track each step's commit, result, and remaining range count in memory.

Repeat Steps 1–5 until only **1 commit** remains in the range (i.e., the good
and bad commits are immediate neighbors: `TOTAL == 1` in
`<GOOD_COMMIT>..<BAD_COMMIT>`).

______________________________________________________________________

## 5. Culprit Commit Inspection & Reporting

Once the search converges, the single remaining bad commit in the range is the
culprit (the first bad commit).

### Inspect the Culprit Commit

Run:

```bash
git log -1 <CULPRIT_COMMIT>
```

Extract:

- **Commit Hash**
- **Git Source Link**:
  `https://chromium.googlesource.com/chromium/src/+/<CULPRIT_COMMIT>`
- **Author**: Name and email (`Author: ...`)
- **Review (Gerrit CL)**: Extracted from `Reviewed-on:` in the commit message or
  `https://chromium-review.googlesource.com/c/chromium/src/+/<cl_number>`
- **Cr-Commit-Position**: Extracted from `Cr-Commit-Position:` (e.g.,
  `refs/heads/main@{#...}`)
- **Title / Subject**: Commit title line
- **Bug**: Extracted from `Bug:` or `Fixed:` line

### Final Report Format

Produce a final report matching the following structure:

#### Template:

```markdown
The bisect has completed successfully and isolated the regression to the first bad commit.

---

### Bisect Summary

| Step | Commit Tested | Result | Range Left |
|---|---|---|---|
| 1 | <commit_hash_1> | <Good|Bad> | <remaining_commits_1> |
| 2 | <commit_hash_2> | <Good|Bad> | <remaining_commits_2> |
| ... | ... | ... | ... |
| N | <commit_hash_N> | <Good|Bad> | 1 |

---

### First Bad Commit (Culprit)

* **Commit**: [<CULPRIT_COMMIT>](https://chromium.googlesource.com/chromium/src/+/<CULPRIT_COMMIT>)
* **Author**: <author_name> (<author_email>)
* **Review**: [crrev.com/c/<cl_number>](https://chromium-review.googlesource.com/c/chromium/src/+/<cl_number>)
* **Commit Position**: `refs/heads/main@{#<position>}`
* **Title**: <commit_title>
* **Bug**: <bug_ID>
```
