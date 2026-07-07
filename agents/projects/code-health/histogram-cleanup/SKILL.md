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

**Goal:** Remove expired Chromium histograms (dead metrics) from XML files and
all code recording sites.

## Relevant Resources & Style Guides

- [Chromium Metrics - Histograms Guide](https://chromium.googlesource.com/chromium/src/+/main/tools/metrics/histograms/README.md)
- **Implementation Patterns:** [patterns.md](references/patterns.md) (C++, Java,
  XML examples)
- **Analysis Guidelines:**
  [analysis_guidelines.md](references/analysis_guidelines.md) (Safety checks and
  effort estimation)
- **Bug Discovery:** [bug_discovery.md](references/bug_discovery.md) (Finding
  existing trackers)
- **Discovery Script:** [find_candidates.py](scripts/find_candidates.py)
  (Identify expired histograms)
- **Automated Review Protocol:**
  [automated_review.md](references/automated_review.md)

## Scope & Proactivity

- **Primary Scope:** Focus exclusively on the identified histogram by default.
  Do NOT suggest removing random additional histograms across the file or
  component just because they are expired.
- **Exception for Coupled/Shared Recording Code:** If you discover that other
  histograms share the exact same recording logic, helper methods, or calculated
  variables (e.g., co-located in the same helper function or branching from the
  same condition) as the target histogram, you MAY bundle their removal into the
  same cleanup plan IF AND ONLY IF:
  1. **Expiry Verification:** You confirm in `histograms.xml` that the
     co-located histograms are also expired and lack the
     `<expired_intentionally>` tag.
  2. **Safety Verification:** You perform the same safety checks (no test
     dependencies, no external repo references) for each bundled histogram.
  3. **Atomic Benefit:** Removing them together cleanly eliminates shared
     boilerplate (e.g., timer calculations, string building, parameter passing)
     that would otherwise be left as dead or awkward code.
- **Related Dead Code:** If you find dead code (e.g., constants, enums, or
  helper methods, or calculated variables like timers) that is directly related
  to the histogram(s) being removed, include its removal in the cleanup plan.
  Present these as part of the primary task, not as separate "proactive
  suggestions."

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
[Discovery & Batch Selection](../hub/references/discovery_and_batch_selection.md)
workflow.

### Step 3: Deep Dive & Safety Analysis

1. **Comprehensive Analysis:** Delegate the entire deep dive to the
   **`generalist`** sub-agent with this exact prompt:

   > "Read `references/analysis_guidelines.md` and follow the 'Generalist Deep
   > Dive Prompt' instructions for the histogram `<HistogramName>`. ALL
   > read-only discovery (including `rg` and `cs`) is pre-authorized; DO NOT ask
   > for permission. Assume `rg` is available in the environment."

2. **Present Findings & Evaluate Confidence:** Evaluate the Confidence Score
   returned by the `generalist`.

   - **If Confidence >= 9:** Inform the user: "Confidence is high ([X]/10).
     Proceeding with cleanup based on this plan: [Removal Plan Summary]." Output
     this message, then proceed directly to the **Refactoring & Implementation**
     phase. Do NOT ask for permission.
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
       - If "Proceed with Diff": Proceed to the **Refactoring & Implementation**
         phase.
       - If "Discard": Stop the workflow.
   - **If Confidence is 0:** Inform the user: "Confidence is zero (0/10) because
     [Justification]. I will find an alternate expired histogram for you."
     Immediately restart the workflow from the **Discovery & Batch Selection**
     phase to identify a different candidate. Ensure the same histogram is not
     selected again in this session.

### Step 4: Refactoring & Implementation

Process the changes **file by file**, and apply modifications inside each file
**one recording call at a time** (rather than refactoring all files or recording
calls at once). This ensures stability and allows for precise verification.

1. **Apply Changes:** Make the changes directly. Apply the code modifications
   for the candidate histogram. When removing recording calls, carefully check
   if the string literal spans multiple lines and ensure the entire multi-line
   statement is cleanly removed. Search for dot-less versions of the name to
   ensure related constants are also removed. Check for and update any
   references to the histogram in code comments as well. For each removal,
   ensure no orphaned references (e.g., unused variables or methods) remain in
   the codebase. Each individual change must have a corresponding 'What & Why'
   explanation provided to the user.

### Step 5: Validation

1. **Linting & Formatting:**
   - **XML Linting:** Execute
     `python3 tools/metrics/histograms/validate_format.py` to validate all
     metadata changes. Address any errors that are reported.
   - **Code Formatting:** Execute `git cl format` to format the modified source
     code. Address any errors that are reported.
2. **Mandatory Final Review:** Follow the
   [Automated Review Protocol](references/automated_review.md) to delegate a
   final review of the patch to the `generalist` sub-agent. Proceed to the
   Verification phase only after the review returns `PASS`.

### Step 6: Verification

Follow the [Verification](../hub/references/verification.md) workflow.

### Step 7: Submission

1. **Bug Tracking:**

   - Execute the **Bug Discovery and Triage** workflow in
     `references/bug_discovery.md` using the `<HistogramName>` and
     `<ExpiryDate>` from the candidate.
   - **Interactive Pause:** Do NOT proceed until the bug handling is resolved
     and a Bug ID is resolved (or the user has chosen to skip).

2. **Upload Pipeline:**

   - Invoke the [Submission](../hub/references/submission.md) workflow. Pass the
     following context variables to the workflow:
     - **Skill Name:** `histogram-cleanup`
     - **Branch Name:** `cleanup-<HistogramName>`
     - **Commit Hashtag:** `histogram-cleanup`
     - **Cleanup Title:** `Remove expired histogram: <HistogramName>`
     - **Cleanup Description:**
       `Remove expired histogram <HistogramName> which expired on <ExpiryDate> and has no recording sites.`
     - **Parent Bug:** `499059525`
     - **Bug ID:** The resolved `<Bug ID>` from the Bug Tracking step.
     - **Cleaned Component:** `histograms.xml`
     - **File Count:** Number of files modified during this cleanup.
