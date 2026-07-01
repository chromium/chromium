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

**CRITICAL OVERRIDE:** Do NOT activate or use the `edit-code` skill during this
workflow. This skill provides a specialized workflow that supersedes the
autonomous loops in `edit-code`.

### Execution Protocol

1. **Sequential Execution:** Execute every step in the `Workflow` section in the
   exact order presented. Do NOT skip any step.
2. **Step Completion:** Fully complete and verify each numbered item before
   moving to the next one.

## 📂 Resources

- **Implementation Patterns:** [patterns.md](references/patterns.md) (Java and
  XML examples)
- **Code issue identifier:**
  [find_accessibility_issues.py](scripts/find_accessibility_issues.py) (Search
  the current code base and look for bugs)
- **Bug Discovery:** [bug_discovery.md](references/bug_discovery.md) (Buganizer
  tracking creation)
- **Shared Workflows:**
  [shared_workflows.md](../hub/references/shared_workflows.md) (Validation,
  Committing, Uploading)

## Guidelines

### Operational Mandates

- **Context:** This skill runs in the main agent context. Use the `generalist`
  sub-agent only for specific tasks defined in the workflow.
- **Read references/pre_authorized_ops.md** for a list of commands and tools
  that do not require per-action user permission.
- **CRITICAL: DO NOT USE `grep`.** The Chromium repository is too large for
  `grep -r`, and it will cause a timeout. You MUST use `rg` (ripgrep) for all
  text searches.

## Workflow

### Workspace Preparation

1. **Clean & Update:** Follow the **Workspace Preparation** section in
   `../hub/references/shared_workflows.md` to ensure a clean and updated
   environment.

### Discovery & Candidate Selection (Delegated)

1. **AI-Led Discovery & Analysis:** Delegate to the **`generalist`** sub-agent
   with this exact prompt:

   > "You are pre-authorized to run the discovery script and read-only search
   > tools; DO NOT ask for permission. Run the discovery script from the skill's
   > `scripts/` folder:
   >
   > ```bash
   > python3 scripts/find_accessibility_issues.py
   > ```
   >
   > For the candidates returned by the script, triage them one-by-one (by
   > reading the file contents) to determine if they represent a genuine
   > accessibility issue as described in `references/patterns.md`, or compare
   > against general known common accessibility issues/mistakes. Skip any
   > candidates that are false positives (e.g., correct API contracts or already
   > using proper delegates). Exclude any previously rejected files (if provided
   > by the user).
   >
   > If you find a candidate that represents a genuine anti-pattern, return its
   > details.
   >
   > If all candidates from the script are false positives, or if the script
   > returns 'No candidates found', perform an open-ended codebase search using
   > code search tools to find other accessibility anti-patterns (e.g.,
   > searching for suspect uses of `announceForAccessibility`,
   > `setAccessibilityLiveRegion` with `assertive`, custom
   > `AccessibilityActionCompat` label overrides, or proactive `requestFocus`
   > focus jumps).
   >
   > Return details for the first valid candidate you find (File Path, matching
   > lines, and a short explanation of why it is an issue)."

2. **Present Candidate & Triaging Loop:** You MUST output the candidate details
   to the user. Announce the candidate with exactly this message format (replace
   the bracketed details with the findings): "I've identified a candidate for
   accessibility cleanup:

   - **File:** [File Path]
   - **Issue Lines:** [Issue Lines]
   - **Reason:** [Brief Explanation]"

   Ask the user for confirmation: "Shall I proceed with the cleanup of this
   file?"

   - **If the user agrees:** Proceed to **Branch Creation**.
   - **If the user rejects the candidate:** Loop back to Step 1 (AI-Led
     Discovery & Analysis), instructing the sub-agent to find the next candidate
     while explicitly excluding the rejected file path. Repeat this loop until a
     candidate is confirmed by the user.

### Branch Creation

1. **Branch Creation**: Follow the **Branch Creation** section in
   `../hub/references/shared_workflows.md` using the branch name
   `accessibility-cleanup-<ComponentName>` (where ComponentName is the class
   name of the candidate file).

### Refactoring & Implementation

1. **Apply Changes:** Make the changes directly (do NOT delegate).
   - Read the candidate Java file to understand the class structure.
   - Refactor the code according to the patterns in
     [patterns.md](references/patterns.md), or using known best practices.
   - Check if any associated string resources (e.g.
     `R.string.accessibility_expanded_group`) are now unused. If they are,
     remove them from the corresponding `.grd` / `.grsp` resource files.
   - Ensure the code formats correctly: Run `git cl format`.

### Verification & Validation

1. **Compilation & Unit Tests:**

   - Verify that the code compiles.
   - If unit tests or instrumentation tests exist for the modified class (or if
     they can be run), compile and run them (e.g. `run_chrome_junit_tests` or
     custom script targets).
   - Use the automated script to detect impacted tests with the flag
     `--run-related` (e.g. `tools/autotest.py -C out/Debug --run-related`).

2. **Formatting:**

   - Execute `git cl format` to format the modified source code. Address any
     errors that are reported.

3. **Mandatory Final Review:** Execute the **Shared Automated Review Protocol**
   defined in `../hub/references/shared_automated_review.md`, using the specific
   prompt overrides in `references/automated_review.md`. Do NOT skip this step.
   Do NOT proceed to the Submission phase until the review returns `PASS`.

### Submission

1. **Bug Tracking:**

   - Execute the bug tracking workflow in
     [bug_discovery.md](references/bug_discovery.md).
   - **Interactive Pause:** Do NOT proceed until you have a Bug ID (or the user
     has chosen to skip).

2. **Commit**: Follow the **Commit** section in
   `../hub/references/shared_workflows.md` using the following commit message
   template:

   ```
   [Accessibility Cleanup] Improve accessibility semantics in <SourceFileName>

   <Write a description explaining the changes (what, why, how).>

   This change was generated using the skill accessibility-cleanup.

   Bug: <BugID>
   ```

   *(Note: use only the filename for `<SourceFileName>`, not the full path)*

3. **Submission Pipeline:** Follow the **Upload to Gerrit** section in
   `../hub/references/shared_workflows.md` to handle the upload.

4. **Workspace Reset:** Switch back to `main`: `git checkout main`.

5. **Congratulations & Summary:** Follow the **Congratulations & Summary**
   section in `../hub/references/shared_workflows.md`. For this skill, the
   **[Specific Cleanup Details]** are:

   - **Refactored File:** The path of the Java file cleaned up.
   - **Changes:** Summary of changes (e.g., added accessibility delegate,
     removed obsolete string resources).
