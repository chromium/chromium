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

## Scope & Proactivity

- **Allowlist (Hard Constraint):** Only histograms defined in one of the
  `histograms.xml` files listed in [safe_dirs.md](references/safe_dirs.md) may
  be examined, selected, bundled, or cleaned up. Never run discovery,
  `scripts/find_expired_in_file.py`, or a deep dive against any other
  `histograms.xml`, and never edit one. If the user names a histogram outside
  the allowlist, explain the restriction and run discovery instead.
- **Primary Scope:** Focus exclusively on the identified histogram by default.
  Do NOT suggest removing random additional histograms across the file or
  component just because they are expired.
- **Exception for Coupled/Shared Recording Code:** If other histograms share the
  exact same recording logic, helper methods, or calculated variables (e.g.,
  co-located in the same helper function or branching from the same condition)
  as the target histogram, their removal MAY be bundled into the same cleanup
  plan IF AND ONLY IF:
  1. **Allowlist Verification:** Confirm each co-located histogram's
     `<histogram>` definition lives in an allowlisted `histograms.xml` from
     `references/safe_dirs.md`. If it does not, it MUST NOT be bundled — leave
     its recording code intact even if shared boilerplate remains.
  2. **Expiry Verification:** Confirm in `histograms.xml` that the co-located
     histograms are also expired and lack the `<expired_intentionally>` tag.
  3. **Safety Verification:** Perform the same safety checks (no test
     dependencies, no external repo references) for each bundled histogram.
  4. **Atomic Benefit:** Removing them together cleanly eliminates shared
     boilerplate (e.g., timer calculations, string building, parameter passing)
     that would otherwise be left as dead or awkward code.
- **Related Dead Code:** If dead code is found (e.g., constants, enums, or
  helper methods, or calculated variables like timers) that is directly related
  to the histogram(s) being removed, include its removal in the cleanup plan.
  Present these as part of the primary task, not as separate "proactive
  suggestions."

## Workflow

> [!IMPORTANT] **Execution Protocol:** Execute Steps 1–7 continuously in one
> autonomous run, without skipping steps or pausing between files or steps. Stop
> only at explicit `ask_question` gates (confidence `1–8` or missing `out` dir)
> or documented interactive stops. Use `rg` (ripgrep) for searches.

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
[Discovery & Batch Selection](../hub/references/discovery_and_batch_selection.md)
workflow.

The discovery plugin (`scripts/find_candidates.py`) only scans and shuffles
across the allowlisted files in [safe_dirs.md](references/safe_dirs.md). Before
presenting any candidate, confirm its `File:` path is in the allowlist; discard
any non-allowlisted or user-supplied out-of-list candidate and repeat discovery.
Only run `scripts/find_expired_in_file.py` on allowlisted files (it refuses
non-allowlisted files with exit code 2).

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
     user using `ask_question` (single choice):
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
     [Justification]. An alternate expired histogram will be found." Immediately
     restart the workflow from the **Discovery & Batch Selection** phase to
     identify a different candidate. Ensure the same histogram is not selected
     again in this session.

### Step 4: Refactoring & Implementation

Process the changes **file by file**, and apply modifications inside each file
**one recording call at a time** (rather than refactoring all files or recording
calls at once). This ensures stability and allows for precise verification.
Process all files and recording calls sequentially within the same run, without
pausing for user confirmation between edits, then proceed directly to Step 5.
Refer to [patterns.md](references/patterns.md) for C++, Java, and XML removal
patterns.

1. **Apply Changes:** Make the changes directly. Apply the code modifications
   for the candidate histogram. Only edit `histograms.xml` files listed in
   `references/safe_dirs.md`. When removing recording calls, carefully check if
   the string literal spans multiple lines and ensure the entire multi-line
   statement is cleanly removed. Search for dot-less versions of the name to
   ensure related constants are also removed. Check for and update any
   references to the histogram in code comments as well. For each removal,
   ensure no orphaned references (e.g., unused variables or methods) remain in
   the codebase. Each individual change must have a corresponding 'What & Why'
   explanation provided to the user.

### Step 5: Formatting & Automated Review

1. **Format & Lint:** Run
   `python3 agents/projects/code-health/hub/scripts/format_and_lint.py` (must
   exit 0).
2. **Automated Review:** Follow
   [Automated Review](../hub/references/automated_review.md), passing
   [review_rules.md](references/review_rules.md) as
   `<SKILL_SPECIFIC_CLEANUP_RULES>`.

### Step 6: Verification

Follow the [Verification](../hub/references/verification.md) workflow.

### Step 7: Submission & Commit Generation

#### 1. Expiry Bug Discovery (Autonomous)

- For each cleaned histogram, query Buganizer for open expiry tracking bugs. For
  patterned histograms containing `{...}` tokens, strip the `{...}` suffix and
  search the un-tokenized name prefix, because expiry bug titles use `.*`
  instead of `{Token}` names:
  `get_bugs_with_external(query='"Check expiry of your histograms" "<UnTokenizedHistogramName>" status:open')`
- Collect the lowest numeric open bug ID found for each cleaned histogram
  (resolving any legacy Monorail IDs if needed) as `<ExpiryBugID>`, or `"none"`
  if none are open. Do NOT prompt the user or file a new bug.

#### 2. Prepare Commit Message

Generate `../commit`. Set `--title` to `Remove expired <Area> histograms` using
the top histogram prefix or XML metadata folder. Keep the full
`[histogram-cleanup][LSC] <Title>` line `<= 72` characters (`<Title>` `<= 47`;
use `Remove expired histograms` if `<Area>` does not fit). Always pass at least
one `--bug`: one repeatable `--bug "<ExpiryBugID>"` per bug found, or
`--bug "none"` if none are open. The script adds the parent bug first, so do not
pass it.

```bash
python3 agents/projects/code-health/hub/scripts/prepare_commit_message.py \
  --skill histogram-cleanup \
  --title "Remove expired <Area> histograms" \
  --bug "<ExpiryBugID>" \
  --description - \
  --out-commit ../commit <<'EOF'
<Brief summary of removed recording sites, dead code, tests, and XML entries.>

OBSOLETE_HISTOGRAMS=<message>
EOF
```

The `OBSOLETE_HISTOGRAMS=<message>` line applies to every removed histogram
(including patterned variants) and must be a single unwrapped line in its own
final paragraph of the description. Do NOT use `OBSOLETE_HISTOGRAMS[Name]=` or
singular `OBSOLETE_HISTOGRAM[Name]=` (including
`OBSOLETE_HISTOGRAM[Name.{Token}]=`, which does not expand patterned variants).
Choose `<message>`:

- **Expired:** `Expired on <ExpiryDate> and no longer recorded.` (or
  `Expired and no longer recorded.` if the dates differ or are missing).
- **Removed before expiry:**
  `<reason it was removed; replaced by <NewHistogram>, if any>.`

#### 3. Validate Commit File

Pass the same `--bug` flags used in Step 7.2 (one `--bug "<ExpiryBugID>"` per
bug found, or `--bug "none"` if none):

```bash
python3 agents/projects/code-health/hub/scripts/prepare_commit_message.py \
  --skill histogram-cleanup \
  --validate ../commit \
  --bug "<ExpiryBugID>"
```

If validation fails, fix the title or description and re-run Steps 7.2–7.3 until
validation exits 0.

#### 4. Submission Pipeline

Follow [submission.md](../hub/references/submission.md), passing:

- **Branch Name:** `cleanup-<HistogramName>` (without `{...}` token braces for
  patterned names)
- **Commit File:** `../commit`

Do NOT pass `--reviewers` or `--cc`.
