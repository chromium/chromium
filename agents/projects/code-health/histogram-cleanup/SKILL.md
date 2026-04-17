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
   > python3 scripts/find_expired.py --count 1
   > ```
   >
   > **Return ONLY the details returned by the script for this candidate**
   > (Name, Owners, Expiry, and Summary)."

2. **Present Candidate:** You MUST output the candidate details to the user.
   Announce the candidate with exactly this message format (replace the
   bracketed details with the findings) before moving to the next step: "I've
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

   - **If Confidence >= 9:** You MUST inform the user: "Confidence is high
     ([X]/10). Proceeding with cleanup based on this plan: \[Removal Plan
     Summary\]." Output this message, then proceed directly to the **Branch
     Creation** phase. Do NOT ask for permission.
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
       - If "Proceed with Diff": Proceed to the **Branch Creation** phase.
       - If "Discard": Stop the workflow.
   - **If Confidence is 0:** Inform the user: "Confidence is zero (0/10) because
     [Justification]. I will find an alternate expired histogram for you."
     Immediately restart the workflow from the **Discovery & Candidate
     Selection** phase to identify a different candidate. Ensure you do not
     select the same histogram again in this session.

### Branch Creation

Inform the user: "Preparing workspace: creating a new branch..."

1. **Branch Creation:** Run `git new-branch cleanup-<HistogramName>` to create a
   new branch for this specific cleanup, ensuring it doesn't chain with previous
   commits.

### Implementation

1. **Apply Changes:** Make the changes directly (do NOT delegate). Apply the
   code modifications for the candidate histogram. When removing recording
   calls, carefully check if the string literal spans multiple lines and ensure
   the entire multi-line statement is cleanly removed. Search for dot-less
   versions of the name to ensure related constants are also removed. Check for
   and update any references to the histogram in code comments as well. For each
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

2. **Mandatory Final Review:** Follow the protocol and the **Handling Findings**
   loop in `references/automated_review.md` for the removal: `<HistogramName>`.
   Do NOT skip this step. Do NOT proceed to the Submission phase until the
   review returns `PASS`.

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
     autonomously stage ONLY the specific files modified during this task using
     `git add` and execute the commit:
     ```bash
     git commit -m "<drafted message>"
     ```

3. **Submission Pipeline:** Follow the **Upload to Gerrit** section in
   `../hub/references/shared_workflows.md` to handle the upload.

4. **Workspace Reset:** Immediately switch back to `main` to start fresh for the
   next cleanup: `git checkout main`.

5. **Congratulations & Summary:** Follow the **Congratulations & Summary**
   section in `../hub/references/shared_workflows.md`. For this skill, the
   **[Specific Cleanup Details]** are:

   - **Removed Histogram:** The name of the removed histogram and its expiry
     year.
