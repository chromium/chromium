---
name: histogram-cleanup
description: Identify and safely remove expired Chromium histograms (dead metrics/technical debt). Use this skill when a contributor asks to clean up metrics, fix code health issues related to histograms, remove obsolete code, or work on a histogram cleanup task.
---

# Code Health: Histogram Cleanup

Identify and safely remove "dead code" associated with expired histograms. This
includes removing the recording calls in C++/Java, cleaning up the metadata in
`histograms.xml`, and addressing any dependent tests.

## Overview

Expired histograms that are not intentionally kept for diagnostics represent
technical debt. Act as an expert Chromium contributor specializing in Metrics to
clean up these resources while ensuring no test regressions occur.

**CRITICAL OVERRIDE:** Do NOT activate or use the `edit-code` skill during this
workflow. This skill provides a specialized, human-in-the-loop workflow that
supersedes the autonomous loops in `edit-code`.

### Execution Protocol

1. **Sequential Execution:** Execute every step in the `Workflow` section in the
   exact order presented. Do NOT skip any step.
2. **Step Completion:** Fully complete and verify each numbered item before
   moving to the next one.

## 📂 Resources

- **Implementation Patterns:** [patterns.md](references/patterns.md) (C++, Java,
  XML examples)
- **Analysis Guidelines:**
  [analysis_guidelines.md](references/analysis_guidelines.md) (Safety checks and
  effort estimation)
- **Bug Discovery:** [bug_discovery.md](references/bug_discovery.md) (Finding
  existing trackers)
- **Pre-authorized Operations:**
  [pre_authorized_ops.md](references/pre_authorized_ops.md) (Authorized commands
  and discovery tools)
- **Shared Workflows:**
  [shared_workflows.md](../hub/references/shared_workflows.md) (Validation,
  Committing, Uploading)

## Guidelines

### Operational Mandates

- **Context:** This skill runs in the main agent context. Use the `generalist`
  sub-agent only for the specific tasks defined in the workflow.
- **Read references/pre_authorized_ops.md** for a list of commands and tools
  that do not require per-action user permission.
- **Modification Consent:** Explicit user permission is **STILL REQUIRED** for
  any operation that modifies the source code (e.g., `replace`, `write_file`) or
  commits changes (`git commit`).

### Scope & Proactivity

- **Strict Scope:** Focus exclusively on the identified histogram. Do NOT
  suggest removing additional histograms, even if they are related or also
  expired.
- **Related Dead Code:** If you find dead code (e.g., constants, enums, or
  helper methods) that is directly and exclusively related to the histogram
  being removed, include its removal in the cleanup plan. Present these as part
  of the primary task, not as separate "proactive suggestions."

## Workflow

### Discovery & Candidate Selection (Delegated)

1. **AI-Led Discovery & Analysis:** Delegate to the **`generalist`** sub-agent
   with this exact prompt:

   > "You are pre-authorized to run the discovery script and read-only search
   > tools; DO NOT ask for permission. Run the discovery script from the skill's
   > `scripts/` folder:
   >
   > ```bash
   > # Note: This path is relative to the repository root
   > python3 agents/projects/code-health/histogram-cleanup/scripts/find_expired.py --count 1
   > ```
   >
   > **Return ONLY the details returned by the script for this candidate**
   > (Name, Owners, Expiry, and Summary)."

2. **Present Candidate:** Announce the candidate to the user with exactly this
   message format (replace the bracketed details with the findings): "I've
   identified an expired histogram for cleanup today:

   - **Name:** [Name]
   - **Owners:** [Owners]
   - **Expiry:** [Expiry]
   - **Summary:** [Summary]"

### Deep Dive & Safety Analysis (Delegated)

1. **Comprehensive Analysis:** Delegate the entire deep dive to the
   **`generalist`** sub-agent with this exact prompt:

   > "Read `references/analysis_guidelines.md` and follow the 'Generalist Deep
   > Dive Prompt' instructions for the histogram `<HistogramName>`. You are
   > pre-authorized for ALL read-only discovery (including `rg` and `cs`); DO
   > NOT ask for permission. Assume `rg` is available in the environment."

2. **Present Findings & Evaluate Confidence:** Evaluate the Confidence Score
   returned by the `generalist`.

   - **If Confidence >= 9:** Inform the user: "Confidence is high ([X]/10).
     Proceeding with cleanup based on this plan: [Removal Plan Summary]."
     Proceed directly to the **Workspace Preparation** phase. Do NOT ask for
     permission.
   - **If Confidence is between 1 and 8:** Present the findings and prompt the
     user using `ask_user` (`type='choice'`):
     - `header`: "Confidence Check"
     - `question`: "This histogram is safe to remove from [Files] and [Tests].
       My confidence for this cleanup is [X]/10 because [Justification]. Shall I
       proceed with the cleanup diff?"
     - `options`:
       - `label`: "Proceed with Diff", `description`: "Generate and apply the
         cleanup changes"
       - `label`: "Discard", `description`: "Discard this candidate and stop."
     - **Action based on selection:**
       - If "Proceed with Diff": Proceed to the **Workspace Preparation** phase.
       - If "Discard": Stop the workflow.
   - **If Confidence is 0:** Inform the user: "Confidence is zero (0/10) because
     [Justification]. I will find an alternate expired histogram for you."
     Immediately restart the workflow from the **Discovery & Candidate
     Selection** phase to identify a different candidate. Ensure you do not
     select the same histogram again in this session.

### Workspace Preparation

Before making any modifications, ensure a clean and isolated environment. Inform
the user: "Preparing workspace: cleaning up local changes and creating a new
branch..."

1. **Handle Local Changes:** Run `git status --porcelain -uno`. If there is any
   output, run `git stash` and inform the user: "I noticed uncommitted changes;
   I've stashed them (`git stash`) to ensure a clean environment for the
   cleanup."
2. **Switch and Update:** Always start fresh from `main`:
   `git checkout main && git pull origin main --rebase && gclient sync -D`
3. **Check for unmerged local commits:** Run `git log origin/main..HEAD`. If
   there is any output, stop and inform the user, as we do not want to carry
   these over to the new branch.
4. **Branch Creation:** Run `git new-branch cleanup-<HistogramName>` to create a
   new branch for this specific cleanup, ensuring it doesn't chain with previous
   commits.

### Implementation

1. **Apply Changes:** Make the changes directly (do NOT delegate). Apply the
   code modifications for the candidate histogram. When removing recording
   calls, carefully check if the string literal spans multiple lines and ensure
   the entire multi-line statement is cleanly removed. Search for dot-less
   versions of the name to ensure related constants are also removed. For each
   removal, ensure no orphaned references (e.g., unused variables or methods)
   remain in the codebase. Each individual change must have a corresponding
   'What & Why' explanation provided to the user.

### Review & Validation

1. **Linting & Formatting:**

   - **XML Linting:** Execute
     `python3 tools/metrics/histograms/validate_format.py` to validate all
     metadata changes. Address any errors that are reported.
   - **Code Formatting:** Execute `git cl format` to format the modified source
     code. Address any errors that are reported.

2. **Mandatory Final Review:** Follow the **Automated Review** section in
   `../hub/references/shared_workflows.md` for the removal: `<HistogramName>`.
   Do NOT skip this step. If feedback is provided, address the issues and re-run
   the review. Do NOT proceed to the Submission phase until the review returns
   PASS.

### Submission

1. **Bug Tracking:**

   - Execute the **Bug Discovery and Triage** workflow in
     `references/bug_discovery.md` using the `<HistogramName>` and
     `<ExpiryDate>` from the candidate.
   - **Interactive Pause:** Do NOT proceed until the bug handling is resolved
     and you have a Bug ID (or the user has chosen to skip).

2. **Commit:**

   - **Draft Message:** Draft a commit message following this template:
     ```
     [histogram-cleanup] Remove expired histogram: <HistogramName>

     This histogram expired on <ExpiryDate>.

     Bug: <BugID>
     ```
   - **Execution:** Display the drafted commit message to the user. Then,
     autonomously execute the following to commit the changes:
     ```bash
     git add -u
     git commit -m "<drafted message>"
     ```

3. **Submission Pipeline:** Follow the **Upload to Gerrit** section in
   `../hub/references/shared_workflows.md` to handle the upload.

4. **Workspace Reset:** Immediately switch back to `main` to start fresh for the
   next cleanup: `git checkout main`.

5. **Congratulations & Summary:** After the task is complete, congratulate the
   user for their contribution to the Chromium project's code health and display
   a brief summary of the cleanup that was performed. The summary MUST include:

   - **Gerrit CL:** The URL of the uploaded changelist.
   - **Bug:** The Bug ID (or "None").
   - **Removed Histogram:** The name of the removed histogram and its expiry
     year.
   - **Modified Files:** A list of all files changed.
