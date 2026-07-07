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

## Relevant Resources & Style Guides

- [Chromium Accessibility Developer Guide](https://www.chromium.org/developers/design-documents/accessibility)
- [Android Accessibility Guide](https://developer.android.com/guide/topics/ui/accessibility)
- **Implementation Patterns:** [patterns.md](references/patterns.md) (Java and
  XML examples)
- **Code issue identifier:** [find_candidates.py](scripts/find_candidates.py)
  (Search the current code base and look for bugs)
- **Discovery Workflow:**
  [discovery_and_batch_selection.md](references/discovery_and_batch_selection.md)
- **Automated Review Protocol:**
  [automated_review.md](references/automated_review.md)

## Workflow

> [!IMPORTANT] **Execution Protocol:** Execute all steps sequentially one by
> one. Do not skip any step. Do not use `edit-code` or `grep`. Use `rg`
> (ripgrep) for searches.

### Step 1: Workspace Preparation

Follow the workspace preparation steps in
[workspace_preparation.md](../hub/references/workspace_preparation.md) to ensure
a clean and updated environment.

### Step 2: Discovery & Batch Selection

Follow the
[Discovery & Batch Selection](references/discovery_and_batch_selection.md)
workflow.

### Step 3: Refactoring & Implementation

Process the candidates by handling them **one file at a time**, and applying
modifications inside each file **one instance at a time** (rather than
refactoring all files or instances at once). This ensures stability and allows
for precise verification.

1. **Apply Changes:** Make the changes directly.
   - Read the candidate Java file to understand the class structure.
   - Refactor the code according to the patterns in
     [patterns.md](references/patterns.md), or using known best practices.
   - Check if any associated string resources (e.g.
     `R.string.accessibility_expanded_group`) are now unused. If they are,
     remove them from the corresponding `.grd` / `.grsp` resource files.
   - Ensure the code formats correctly: Run `git cl format`.

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
   - **Interactive Pause:** Do NOT proceed until the bug handling is resolved
     and a Bug ID is resolved (or the user has chosen to skip).

### Step 7: Submission

Invoke the [Submission](../hub/references/submission.md) workflow. Pass the
following context variables to the workflow:

- **Skill Name:** `accessibility-cleanup`
- **Branch Name:** `cleanup-a11y-<ComponentName>` (where ComponentName is the
  class name of the candidate file)
- **Commit Hashtag:** `Accessibility Cleanup`
- **Cleanup Title:** `Improve accessibility semantics in <SourceFileName>`
- **Cleanup Description:**
  `<Write a concise description explaining the changes (what, why, how) for the commit message.>`
- **Parent Bug:** `531299755`
- **Bug ID:** The resolved `<Bug ID>` from the Bug Tracking step.
- **Cleaned Component:** `<SourceFileName>`
- **File Count:** `1`
