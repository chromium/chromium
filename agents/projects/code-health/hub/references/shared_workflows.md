# Shared Code Health Workflows

Use these instructions to handle generic validation and submission steps for any
Code Health cleanup task.

## Table of Contents

- [Pre-authorized Operations](#pre-authorized-operations-generic)
- [Commit](#commit)
- [Upload to Gerrit](#upload-to-gerrit)

## Pre-authorized Operations (Generic)

The following operations are pre-authorized for all Code Health tasks:

- **Read-Only Discovery:** `rg`, `cs`, `ls`, `fdfind`, `glob`, `cat`, and
  `read_file`.
- **Validation & Prep:** `git cl format`,
  `git pull origin main --rebase > /dev/null 2>&1`,
  `gclient sync -D > /dev/null 2>&1`, `git status`, `git stash`, `git checkout`,
  `git log`, `git new-branch`, `git add`, `git commit`.
- **Submission:** `git cl upload --force --bypass-hooks -a -d`.

## Workspace Preparation

Before making any modifications or running discovery scripts, ensure a clean and
isolated environment.

1. **Handle Local Changes:** Run
   `git status --porcelain -uno --ignore-submodules`. If there is any output,
   run `git stash` and inform the user: "I noticed uncommitted changes; I've
   stashed them (`git stash`) to ensure a clean environment."
2. **Switch and Update:** Always start fresh from `main`:
   `git checkout main && git pull origin main --rebase > /dev/null && gclient sync -D > /dev/null 2>&1`
3. **Check for unmerged local commits:** Run `git log origin/main..HEAD`. If
   there is any output, stop and inform the user, as we do not want to carry
   these over to a new branch.

## Upload to Gerrit

**CRITICAL MANDATE:** You MUST execute these commands autonomously immediately
after committing. Do NOT ask for permission.

1. **Prep Workspace:** Run
   `git pull origin main --rebase > /dev/null 2>&1 && gclient sync -D > /dev/null 2>&1`.
2. **Upload to Gerrit:** Run `git cl upload --force --bypass-hooks -a -d`.
3. **Address Presubmit Feedback:** If
   `git cl upload --force --bypass-hooks -a -d` fails due to presubmit errors or
   provides warnings, you MUST analyze the output, fix the issues in the code,
   and re-attempt the upload. Do NOT bypass these checks.

## Congratulations & Summary

After the task is complete, congratulate the user for their contribution to the
Chromium project's code health and display a brief summary of the work
performed. The summary MUST include:

- **CL:** \[Full URL extracted using
  `git cl issue | awk '{print $4}' | tr -d '()'`\]
- **Tracking bug:** b/<BugID> (or "None")
- **\[Specific Cleanup Details\]:** (e.g., what changed (WTC), removed
  histograms, synced enums)
- **Modified Files:** A list of all files changed.
