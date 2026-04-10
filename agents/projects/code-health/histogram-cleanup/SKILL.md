---
name: code-health-histogram-cleanup
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

## 📂 Resources

- **Implementation Patterns:** [patterns.md](references/patterns.md) (C++, Java,
  XML examples)
- **Analysis Guidelines:**
  [analysis_guidelines.md](references/analysis_guidelines.md) (Safety checks and
  effort estimation)
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

- **Strict Scope:** NEVER make changes unrelated to histogram cleanup without
  explicit permission from the user.
- **Proactive Suggestions:** If you notice potential improvements during
  analysis (e.g., an unused enum or surrounding dead code), present these
  observations to the user and ask for permission to address them.

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
   > python3 agents/skills/code-health-histogram-cleanup/scripts/find_expired.py --count 3
   > ```
   >
   > **Return ONLY the details returned by the script for these 3 candidates**
   > (Name, Owners, Expiry, Recording Sites, Effort Level, and Summary) to the
   2. **Present Candidates:** First, **display the full detailed list** (Name,
      Owners, Expiry, Recording Sites, Approx Effort Level, Summary) returned by
      the `generalist` as clear **bullet points** (one for each candidate) so
      the user can easily read the context. Then, prompt the user using
      `ask_user` (`type='choice'`):
      - `header`: "Select Histogram"
      - `question`: "Which histogram would you like to start cleaning up?"
      - `options`: Provide one option for each of the 3 candidates with `label`
        as the histogram name and `description` as its Approx Effort Level
        (e.g., `label`: "<Name>", `description`: "🟢 Easy"), PLUS a 4th option
        (`label`: "Show more ... options", `description`: "Fetch the next 3
        candidates").
2. **Interactive Pause:**
   - If "Show more options" is selected, delegate to the **`generalist`**
     sub-agent to run the discovery script again and fetch the next batch of
     candidates, then repeat the presentation.
   - Do not proceed to the deep dive analysis until a specific histogram is
     selected.

### Deep Dive & Safety Analysis (Delegated)

Once a candidate is selected:

1. **Comprehensive Analysis:** Delegate the entire deep dive to the
   **`generalist`** sub-agent with this exact prompt:
   > "Read `references/analysis_guidelines.md` to understand the 'Safety Checks'
   > criteria. Perform an exhaustive safety analysis for the removal of the
   > expired histogram `<HistogramName>`. You are pre-authorized for ALL
   > read-only discovery (including `rg` and `cs`); DO NOT ask for permission.
   >
   > 1. **Search:** Find ALL occurrences of this histogram string (including any
   >    expanded `<token>` or `<variants>` generated names) in the codebase.
   >    **Prefer `rg` for thorough local searching.** Identify ALL recording
   >    sites (C++, Java, Objective-C) and references in tests.
   > 2. **Safety Verification:** Strictly follow the 'Safety Checks' section in
   >    the guidelines to identify test dependencies, shared enums, and
   >    intentional expiry tags.
   > 3. **Scoring:** Based on your findings and the guidelines, calculate a
   >    Confidence Score (1-10) for its safe removal. (10/10 = 1-2 places, no
   >    test dependencies; < 7/10 = multiple sites, complex mocks). **Return
   >    ONLY a concise summary of the affected files and tests, any identified
   >    risks, the final Confidence Score, and a brief justification.**"
2. **Present Findings:** Display the impacted files, tests, identified risks,
   and Confidence Score returned by the `generalist` to the user so they can
   review the analysis.
3. **Interactive Pause:** HALT AND WAIT FOR USER SELECTION. Prompt the user
   using `ask_user` (`type='choice'`):
   - `header`: "Confidence Check"
   - `question`: "This histogram is safe to remove from [Files] and [Tests]. My
     confidence for this cleanup is [X]/10 because [Justification]. Shall I
     proceed with the cleanup diff?"
   - `options`:
     - `label`: "Proceed with Diff", `description`: "Generate and apply the
       cleanup changes"
     - `label`: "Discard & Pick Another", `description`: "Discard this candidate
       and return to candidate selection"
   - **Action based on selection:**
     - If "Proceed with Diff": Proceed to the **Implementation & Verification**
       steps.
     - If "Discard & Pick Another": Return to the **candidate presentation**
       step to show the remaining candidates from the previously fetched list
       (or fetch more if needed).

### Implementation & Verification

**Announce Status:** Briefly inform the user that you are beginning the
Implementation & Verification steps before starting the next step.

1. **Apply Changes:** Make the changes directly (do NOT delegate). Make the
   changes one by one. Each individual change must have a corresponding 'What &
   Why' explanation provided to the user.

2. **Validation & Formatting:** Follow the **XML Linting** and **Code
   Formatting** steps in the
   [Shared Workflows](../hub/references/shared_workflows.md).

3. **Batching Opportunity:** Prompt the user using `ask_user` (`type='choice'`)
   to see if they want to clean up more histograms from the SAME XML file:

   - `question`: "I've modified the XML file. Would you like me to scan this
     file for other expired histograms to batch into this change?"
   - `options`:
     - `label`: "Scan for more", `description`: "Find other expired histograms
       in the current XML file"
     - `label`: "Finish this file", `description`: "Proceed to final validation
       of current changes"

   **Action based on selection:**

   - If "Scan for more": Delegate to the **`generalist`** to find more expired
     histograms in that specific file and return to the **candidate
     presentation** step with these new candidates.
   - If "Finish this file": Proceed to Step 4.

4. **Automated Review (Delegated):** Before asking the user to validate,
   delegate to the **`generalist`** sub-agent to perform a final review.
   Delegate with this prompt:

   > "Perform an 'Automated Review' for the removal of `<HistogramName>`
   > following the criteria and execution instructions in
   > `../hub/references/shared_workflows.md`. **Return ONLY 'PASS' or a concise
   > list of identified issues.**"

   **Action based on feedback:**

   - If 'PASS': Proceed to **Step 5: User-Led Validation**.
   - If feedback is provided: Address the issues and re-run this review.

5. **User-Led Validation:** STOP. Do not run build/test commands yourself.
   Prompt the user using `ask_user` (`type='choice'`):

   - `question`: "Please validate locally. Run the build for the specific
     targets that include the files we modified (e.g.,
     `autoninja -C out/Default components_unittests`) and run the relevant
     tests. What is the result?"
   - `options`:
     - `label`: "Passed", `description`: "Build and tests are successful;
       proceed to commit drafting"
     - `label`: "Failed", `description`: "Encountered errors that need analysis
       and fixing"
   - **Action based on selection:**
     - If Passed: Proceed sequentially to **Step 6: Bug Tracking**.
     - If Failed: Analyze the error and propose a fix.

6. **Bug Tracking:**

   - Follow the **Bug Tracking** section in the
     [Shared Workflows](../hub/references/shared_workflows.md) to handle bug
     discovery and creation, using these parameters:
     - **`<SearchQuery>`**:
       `"Check expiry of your histograms: <HistogramName>" "<ExpiryDate>"`
     - **`<TaskTag>`**: `histogram-cleanup`
     - **`<ShortSummary>`**: `Remove expired histogram <name>`
   - **Interactive Pause:** Do NOT proceed until the bug handling is resolved
     and you have a Bug ID (or the user has chosen to skip).

7. **Commit Message Drafting:**

   - Follow the **Commit Message Drafting** section in the
     [Shared Workflows](../hub/references/shared_workflows.md) to draft the
     commit message, passing the resolved `<BugID>` (or "skip").
   - Include this mandatory task-specific footer:
     - **Required Footer:** `OBSOLETE_HISTOGRAM[<name>]=<message>` *Example:*
       `OBSOLETE_HISTOGRAM[My.Metric.Name]=The feature was removed, so this metric is no longer being recorded.`
   - **Display the Draft:** You MUST output the drafted commit message in a
     markdown code block so the user can review it.

8. **Submission Pipeline:** Follow the **Interactive Commit** and **Upload to
   Gerrit** sections in the
   [Shared Workflows](../hub/references/shared_workflows.md) to handle the
   interactive commit and Gerrit upload.

9. **Congratulations:** After the task is complete, congratulate the user for
   their contribution to the Chromium project's code health.
