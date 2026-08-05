---
name: multi-agent-release-manager
description: >-
  Cleans up the workspace, formats code, runs presubmit checks, and uploads CLs
  to Gerrit.
---

# Multi-Agent Release Manager Protocol

This skill is a single-agent utility designed to handle workspace hygiene, code
formatting, final validation, and CL deployment for both Jujutsu (JJ) and Git
environments.

## Stage 0: Grounding & Validation

1. **Discover Environment:** Read `project.magi.json` to ground the VCS (`GIT`
   or `JJ`), repo type (`CHROMIUM` or `GOOGLE_INTERNAL`), and `temp_directory`.
   If `project.magi.json` is missing, default to `GIT` and search for `.git` or
   `.jj` directories to verify.
2. **Format Code:**
   - Run `git cl format` (or equivalent formatter for the repository type).
3. **Validate Build & Tests:**
   - Run `git cl presubmit` if in Chromium repository.
   - Run the specified `build_targets` or unit tests defined in
     `project.magi.json` to ensure no last-minute formatting bugs broke the
     build.
   - If tests fail, report the failures and abort the release. Do NOT upload.

## Stage 1: Deploy CLs

1. **Determine Tag and Hashtag:** Read `project.magi.json#active_protocol` to
   determine the tagging:
   - `MAGI` -> Tag: `TAG=magi`, Hashtag: `magi`
   - `TDD` -> Tag: `TAG=magi-tdd`, Hashtag: `magi-tdd`
   - `REVIEW` -> Tag: `TAG=magi-review`, Hashtag: `magi-review`
   - If `active_protocol` is missing, do not add these tags/hashtags.
   - Note: Global rule `TAG=agy` and `CONV=<conversation_id>` must always be
     appended to the description footer.

Based on the VCS detected:

### jujutsu (JJ) Workflow

1. Run `jj status` to identify modified files.
2. **Feature CL (Product Changes):**
   - Identify all files modified under product source directories.
   - Separate them into their own change (rooted at `main`). Use `jj split` or
     `jj squash -i` if they are mixed with MAGI files.
   - Describe the change. Append the determined Tag (e.g., `TAG=magi`) and
     global tags (`TAG=agy`) to the commit message footer.
   - Upload the change: `jj upload` (or equivalent upload command) with the
     determined Hashtag (e.g., `magi`).
3. **MAGI Upgrades CL (Config Changes - If Applicable):**
   - If `personas/**/*.json` or `ROUTING.md` were modified (e.g., by training),
     create a sibling change rooted at `main`.
   - Stage *only* the MAGI files in this change.
   - Describe the change. Append the determined Tag (e.g., `TAG=magi`) and
     global tags (`TAG=agy`) to the footer.
   - Upload the change with the determined Hashtag (e.g., `magi`).

### Git Workflow

1. Run `git status` to identify modified files.
2. **Feature CL (Product Changes):**
   - Create a clean branch for the feature if not already on one.
   - Stage *only* the product source changes: `git add <product_files>`. Do NOT
     stage any `agents/skills/` changes.
   - Commit the changes. Append the determined Tag (e.g., `TAG=magi`) and global
     tags (`TAG=agy`) to the footer of the commit message.
   - Upload the CL: `git cl upload --hashtag <hashtag>` (using the determined
     Hashtag).
   - Push description: `git cl description -n +`.
3. **MAGI Upgrades CL (Config Changes - If Applicable):**
   - If there are modified files in `personas/**/*.json` or `ROUTING.md`, create
     a separate branch.
   - Stage *only* the MAGI configuration files: `git add <magi_files>`.
   - Commit the changes. Append the determined Tag (e.g., `TAG=magi`) and global
     tags (`TAG=agy`) to the footer.
   - Upload the CL: `git cl upload --hashtag <hashtag>` (using the determined
     Hashtag).
   - Push description: `git cl description -n +`.

## Stage 2: Workspace Cleanup

1. **Revert Leftovers:** Revert any untracked or modified files that were not
   part of the uploaded CLs (e.g., accidental submodule bumps).
2. **Delete Temp Files:** Find and delete all temporary files generated during
   the process (e.g., files matching `*.magi`, `*.magi.*` in the repository).
3. **Delete Temp Directory:** Delete the configured `temp_directory`
   recursively.
4. **Final Status:** Report the uploaded CL URLs and confirm that the workspace
   is clean.

## Evaluation & Testing

When modifying this skill's workflow, routing, or schemas, ensure that the
corresponding Promptfoo evaluation test suite is updated and passing:

- [eval.promptfoo.yaml](../../prompts/eval/multi-agent-release-manager/eval.promptfoo.yaml)
