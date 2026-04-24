---
name: jj
description: >-
  Manage Chromium workspaces using jj (Jujutsu) version control. Covers setup,
  Gerrit integration, splitting, conflict resolution, and history reshaping.
---

# jj (Jujutsu) Version Control

jj is an open-source git-compatible VCS (https://github.com/jj-vcs/jj) that can
be used for version control in a Chromium git workspace.

## Table of Contents

- [General Tips](#general-tips)
- [Core Concepts](#core-concepts)
- [Installation](#installation)
- [Workspace Setup](#workspace-setup)
- [CLI Quick Reference](#cli-quick-reference)
- [Common Workflows](#common-workflows)
- [Changelist Management (Critique)](#changelist-management-critique)
- [Splitting Commits](#splitting-commits)
- [History Reshaping](#history-reshaping)
- [Conflict Resolution](#conflict-resolution)
- [Moving and Copying Files](#moving-and-copying-files)
- [Formatting and Linting](#formatting-and-linting)
- [Advanced Workflows](#advanced-workflows)
- [Google-Specific Revsets](#google-specific-revsets)
- [Constraints](#constraints)

## General Tips


Most `jj` commands are non-interactive by default. For commands that accept
`-i` / `--interactive` (e.g. `jj split -i`, `jj squash -i`), omit the flag
for scripted/automated use. Use `-m` with `jj describe` and `jj commit` to
set descriptions non-interactively.

## Core Concepts

### Change IDs vs Commit IDs

Every commit has two IDs:

*   **Change ID** (letters k-z): Stable across rewrites. Use these for
    references. Short unique prefixes work (e.g. `tzntmmzu`).
*   **Commit ID** (hex): Changes on every rewrite. Refers to a specific
    incarnation.

Always favor Change IDs over Commit IDs. Use `CHANGEID/N` syntax if conflicts
occur.

### The Working Copy Is a Commit

The working copy is a regular commit shown as `@` in `jj log`. Every `jj`
command implicitly amends it if the working copy changed. New files are
automatically tracked; deleted files are automatically removed.

**Important:** Because changes auto-amend into `@`, it's easy to accidentally
mix unrelated work into the current commit. Create a new commit with `jj new`
when starting new work rather than editing in-place.

### First-Class Conflicts

Commits can represent conflicted states. Rebases always succeed — conflicts are
recorded in the commit and can be resolved later. jj automatically rebases
descendants when you amend or abandon a commit.

Conflicted commits are marked `(conflict)` in `jj log`. Editing a file to remove
conflict markers is sufficient — there is no separate "mark resolved" step.
Resolving a conflict in an ancestor automatically rebases descendants, but may
introduce new conflicts in them; always resolve bottom-up (oldest conflicted
commit first).

### Operation Log and Undo

The entire repo is versioned in an operation log:

*   `jj undo` — undo the last operation (repeat for multiple undos)
*   `jj redo` — undo an undo
*   `jj op log` — view operation history
*   `jj op restore <OP_ID>` — restore to a specific operation

## Installation

Follow the upstream installation instructions at:
https://docs.jj-vcs.dev/latest/install-and-setup/

Googlers should follow the internal instructions at: go/jj

## Workspace Setup

### Chromium specific configuration

Follow the instructions at [tools/jj/README.md](tools/jj/README.md).

### Detecting jj Workspaces

```bash
jj root
```

### Initializing a jj workspace in a colocated git repository

```bash
jj git init
```

## Remote Head and Syncing

`main@origin` is usually the head of the main branch in the remote repository
when you last synced.

```bash
jj sync        # fetches remote changes and sync current branch to main@origin
```

## CLI Quick Reference

Goal                  | Command
:-------------------- | :-------------------------------------------------------
Status                | `jj status`
History               | `jj log`
Show commit           | `jj show [REV]`
Show with summary     | `jj show --summary`
Diff                  | `jj diff --git`
Diff (stat)           | `jj diff --stat`
Diff (names only)     | `jj diff --name-only`
Diff (specific rev)   | `jj diff --git -r <REV>`
Diff (from base)      | `jj diff --git --from "trunk()"`
File list             | `jj file list <path>`
File at revision      | `jj file show -r <REV> <file>`
File blame            | `jj file annotate <file>`
New commit            | `jj new [PARENT]`
Describe              | `jj describe -m "msg"`
Commit (describe+new) | `jj commit -m "msg"`
Squash into parent    | `jj squash`
Split                 | `jj split [FILES]`
Rebase                | `jj rebase -d <DEST> -s <SOURCE>`
Abandon               | `jj abandon [REV]`
Restore file          | `jj restore <file>`
Restore from rev      | `jj restore --from <REV> <file>`
Duplicate commit      | `jj duplicate -r <REV>`
Untrack file          | `jj file untrack <file>`
Format                | `jj fix`
Upload CL             | `jj upload`
Mail for review       | `jj upload -s`
Submit CL             | `jj upload -c`
Import CL             | `git cl patch <CL_NUMBER>`
Undo                  | `jj undo`
Op history            | `jj op log`
Change evolution      | `jj evolog -r <REV>`

## Common Workflows

### Starting New Work

```bash
jj new main@origin        # new commit on top of main@origin (jj equivalent for git's origin/main)
# ... make changes ...
jj describe -m "Add feature X"
```

### Extending a Chain

```bash
jj new
# ... make changes ...
jj describe -m "Follow-up change"
```

Alternatively, use `jj commit` which describes changes and automatically
starts a new empty commit on top:

```bash
# Starting from an empty working copy commit
# ... make changes ...
jj commit -m "Follow-up change"
# ... make more changes on the automatically created new commit ...
jj commit -m "Next follow-up"
```

### Modifying an Earlier Commit (Fixup)

Prefer `jj new` + `jj squash` over `jj edit` to avoid accidentally mixing
changes:

```bash
jj new <REV>                                # create child of the target commit
# ... make fixes ...
jj squash --into <REV> -m "new commit desc" # fold changes into the target
```

If you just left a commit and want to return to it, `jj edit <REV>` is
acceptable.

### Describing Commits

Always read the current description before modifying:

```bash
jj show --summary

# Use a single -m argument for multiline descriptions.
# Do NOT use multiple -m args; jj only accepts one.
jj describe -m "Summary line

Body text explaining the change.

Bug: 12345
Test: unit"
```

> [!IMPORTANT]
> **Preserve all existing footers** including `Bug:`, `Test:`, and `Change-Id:`
> lines when updating descriptions.

Commands like `jj describe -m`, `jj commit -m`, and `jj squash -m` **overwrite**
the full description. They will drop existing footers unless you explicitly
include them in the new message.

Follow Chromium specific instructions for writing CL descriptions using the
[cl-description](agents/skills/cl-description) skill.

### Reverting Files

```bash
jj restore <file>                       # revert file to parent version
jj restore --from main@origin <file>    # revert to upstream version
jj restore --from <REV> <file>          # revert to a specific revision
jj abandon                              # discard all working-copy changes
```

### Navigating Commits

To navigate between commits, use `jj edit <REV>` to move your working copy to a
specific commit, or `jj new <REV>` to start new work on top of it.

## Gerrit Integration

### Uploading

`jj upload` uploads changes to Chromium Gerrit.

```bash
jj upload
jj upload -r <REV>
```

If no revisions are provided, this will upload the chain of commits starting at
the current commit `@`, unless it is empty and descriptionless, in which case it
will upload `parents(@)`.

### Mail (send for code review)

`jj upload -s` uploads changes to Chromium Gerrit and mails reviewers.

```bash
jj upload -s
jj upload -s -R <REVIEWERS>
```

Note that <REVIEWERS> must contain full email addresses for reviewers as in the
OWNERS files.

### Submitting

`jj upload -c` uploads changes to Chromium Gerrit with the CQ bit set.

```bash
jj upload -c
jj upload -c -r <REV>
```

### Seeing Changes Since Last Upload

TODO

### Importing a CL

`git cl patch <CL_NUMBER or REVIEW_URL>` where <CL_NUMBER> is the CL number from
the Gerrit review URL patches the CL from Gerrit into the local workspace.

## Splitting Commits

### Fast Path (Top-Down)

Peel off files into the parent commit; remaining changes stay in the child:

```bash
jj split -m "Changes to foo" path/to/foo
jj split -m "Changes to bar" path/to/bar
jj commit -m "Integrate foo and bar changes"
```

### Parallel Split

Create sibling commits instead of parent-child:

```bash
jj split --parallel -m "Fix foo bug" path/to/foo
# Foo fix is now a sibling of the remaining changes
```

### Splitting Commits

Select specific files:

```bash
jj split -m "description for first half" path/to/file1 path/to/file2
```

**Avoid using `jj split -i` in non-interactive environments as it will hang.**
Use `jj split -m` with specific paths instead.

### Extraction (Bottom-Up)

Move specific files into a new commit inserted before the current one:

```bash
jj squash -d @- -m "Refactor foo constants" path/to/foo/constants.h
```

### Validation After Splitting

Every commit in the chain must build and pass tests. Verify the split is
lossless:

```bash
jj diff --git --from <ORIGINAL_BASE> --to @    # should be empty
```

## History Reshaping

### Parallelize a Linear Chain

```bash
jj parallelize @--::@     # 3 linear commits -> 3 siblings
```

### Absorb Changes Into Ancestors

Automatically distribute working-copy changes to the ancestor commits where
those lines were last modified:

```bash
jj absorb
```

### Moving commits around

```bash
jj rebase --source <src> --onto <dest> # onto can be repeated to make src into a merge commit
jj rebase --source <src> --insert-before <target> # Put commit before another
jj rebase --source <src> --insert-after <target> # Put commit after another
jj rebase --source <src> --onto parents(<src>) --onto <dest> # Add dest as extra parent of src
```

### Move Changes Between Commits

```bash
jj squash --from <src> --to <dst> file1.cc file2.h
```

### Combine (Fold/Squash) Commits

```bash
jj squash --into <rev1> --from <rev2>::<revN>    # combine revisions into one
```

### Duplicate a Commit

```bash
jj duplicate -r <REV> --onto <destination>
```

## Conflict Resolution

Conflicts are stored in the commit and can be resolved later.

```bash
jj status       # shows (conflict) on commits
jj new <conflicted_commit>               # create child to resolve in
# ... edit files to remove markers ...
jj squash -m "new commit description"         # fold resolution into parent
```

Resolution tools:

```bash
jj resolve --tool :ours file.txt
jj resolve --tool :theirs file.txt
jj resolve --tool :union file.txt
```

## Formatting

The Chromium specific configuration provides a `jj fix` command which runs
various formatters directly on changed files.

```bash
jj fix
```

Note that `jj upload` automatically runs `jj fix` unless the `--no-fix` flag
is specified.

## Advanced Workflows

### Checkpointing

To take a checkpoint, run:

```bash
jj log --no-graph -T "commit_id.short()" -r @
```
It will print the current short commit id.

You can restore the checkpoint later with

```bash
jj restore --from <COMMIT_ID> --to @  # --to can be changed if you want to restore to a different commit
```

If you do need to restore a checkpoint, but you forgot to checkpoint, you can use `jj evolog -r <REV>` to find old versions of commits, and binary search through them with `jj diff --from <CHANGE_ID>/x --to <CHANGE_ID>` to find the right commit id to restore.

### Testing Multiple Changes Together (Merge Commits)

```bash
jj new change1 change2 change3    # creates a merge commit
# Run unittests
```

## Constraints

*   **NEVER** run `jj log -r 'all()'`, `jj log -r ::<anything>` or `jj log -r
    '::'` in Chromium since it will take long.
*   **ALWAYS** scope revsets to avoid scanning the entire Chromium Git history.
    The default behaviour of `jj log` is
    `-r 'present(@) | ancestors(immutable_heads().., 2) | trunk()'`, which is a
    good start.
*   **ALWAYS** use the `--git` flag for `jj diff` to ensure git-compatible diff output.
*   **NEVER** run `jj upload` without explicit user
    confirmation.
*   **NEVER** run `jj squash` (without `--into`, `--from`, or `--to`) without
    explicit user confirmation, as the default squashes the working copy into
    its parent.
*   **ALWAYS** preserve existing metadata tags (`Bug: `, `Test: `, etc.) when
    updating descriptions and ensure tags are at the very end. Note that
    `jj describe -m` and `jj commit -m` **overwrite** the full description —
    always read the current description first.
*   **ALWAYS** use relative paths from your current directory.
*   **ALWAYS** run `jj new` before starting unrelated work — especially after a
    submit. The working copy auto-amends into `@`; without `jj new`, edits
    silently land in the previous (possibly submitted) commit.
*   **ALWAYS** place options and flags **before** the `--` fileset separator.
    -   ✅ **CORRECT**: `jj diff --git -r @- -- my/file.txt`
    -   ❌ **INCORRECT**: `jj diff -r @- -- my/file.txt --git`
*   If you make a mistake, use `jj undo` immediately rather than complex graph
    manipulation.
*   Prefer `jj split` over `jj new` + `jj restore` loops for efficiency.
