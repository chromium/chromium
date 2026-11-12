---
name: accessibility-cleanup
description: Finds common violations of the Android accessibility API in the Clank App Java code and attempts to address them (e.g. hardcoded state change announcements, missing events, focus moving, assertive live regions, etc).
---

# Code Health: Accessibility Cleanup

Identify and fix accessibility bugs in Android Java files where developers are
inadvertently violating the expected contract of the Android accessibility API.

## Overview

The Android Accessibility API has many subtle rules that have changed over time.
Developers often write custom accessibility code based on the end user result
when testing with TalkBack, but don't actually honor the API contract which
would make the feature work with TalkBack and any other running assistive
technology. The goal of this work is to identify these common issues and anti-
patterns and proactively fix them in the code.

**Goal:** Identify common violations of the Android accessibility API and fix
them in Clank Java files.

## Workflow

> [!IMPORTANT] **Execution Protocol:** Execute all steps continuously in one
> autonomous run, without skipping steps or pausing between files or steps. Stop
> only at explicit `ask_question` gates or documented interactive stops. Use
> `rg` (ripgrep) for searches.

### Step 1: Workspace Preparation

Run the workspace preparation script:

```bash
python3 agents/projects/code-health/hub/scripts/prepare_workspace.py --stash
```

- If the script outputs `(stashed local changes)`, inform the user.
- If the script exits non-zero, stop and report the error instead of continuing
  on a dirty or diverged tree.

### Step 2: Discovery & Batch Selection

Follow the
[Discovery & Batch Selection](references/discovery_and_batch_selection.md)
workflow.

### Step 3: Refactoring & Implementation

Process the candidates by handling them **one file at a time**, and applying
modifications inside each file **one instance at a time** (rather than
refactoring all files or instances at once). This ensures stability and allows
for precise verification. Process all files and instances sequentially within
the same run, without pausing for user confirmation between edits, then proceed
directly to Step 4.

1. **Apply Changes:** Make the changes directly.
   - Read the candidate Java file to understand the class structure.
   - Refactor the code according to the patterns in
     [patterns.md](references/patterns.md), or using known best practices.
   - Check if any associated string resources (e.g.
     `R.string.accessibility_expanded_group`) are now unused. If they are,
     remove them from the corresponding `.grd` / `.grsp` resource files.

### Step 4: Validation

1. **Code Formatting:** Execute `git cl format` to format the modified source
   code. Address any errors that are reported.
2. **Mandatory Final Review:** Follow the
   [Automated Review Protocol](references/automated_review.md) to delegate a
   final review of the patch to the `generalist` sub-agent. Proceed to the
   Verification phase only after the review returns `PASS`.

### Step 5: Verification

Follow the [Verification](../hub/references/verification.md) workflow.

### Step 6: Bug Tracking

1. **Bug Discovery & Creation:** Execute the bug tracking workflow in
   [bug_discovery.md](references/bug_discovery.md) using the `<SourceFileName>`
   and `<SourceFilePath>` from the candidate.
   - **Interactive Pause:** Use `ask_question` to confirm whether to file a bug
     or set `<Bug ID>` to `"none"` to skip. Do NOT proceed to Step 7 until
     `<Bug ID>` is resolved.

### Step 7: Submission

#### 1. Prepare Commit Message

Ensure the full `[Accessibility Cleanup] <Title>` subject is at most 72
characters (omit `.java` or shorten the title, e.g.
`Fix accessibility in <Component>`, if needed), and generate `../commit`. Do not
add `[LSC]` (this skill is not part of the LSC).

```bash
python3 agents/projects/code-health/hub/scripts/prepare_commit_message.py \
  --skill accessibility-cleanup \
  --title "Improve accessibility semantics in <Component>" \
  --bug "<Bug ID>" \
  --description - \
  --out-commit ../commit <<'EOF'
<Concise description explaining what changed, why, and how.>
EOF
```

#### 2. Validate Commit File

```bash
python3 agents/projects/code-health/hub/scripts/prepare_commit_message.py \
  --skill accessibility-cleanup \
  --validate ../commit \
  --bug "<Bug ID>"
```

If validation fails, fix the commit title or description and re-run Steps 1–2
until validation exits 0.

#### 3. Submit CL

Invoke the [Submission](../hub/references/submission.md) workflow, passing:

- **Branch Name:** `cleanup-a11y-<ComponentName>` (where `ComponentName` is the
  class name of the candidate file)
- **Commit File:** `../commit`
