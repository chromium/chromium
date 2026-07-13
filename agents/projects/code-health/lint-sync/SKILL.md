---
name: lint-sync
description: Add missing LINT.IfChange(...) / LINT.ThenChange(...) guards to enums in C++/Java and XML to keep them in sync. Trigger this skill ONLY when a contributor explicitly asks to add lint guards or synchronize enums using LINT guards.
---

# Code Health: Lint Sync Guards

Identify enums in the codebase that are persisted to logs (e.g., recorded in
`enums.xml`) but are missing the `LINT.IfChange(...)` and `LINT.ThenChange(...)`
guards that enforce synchronization between the source code and the XML
metadata.

## Overview

When developers modify an enum in C++ or Java but forget to update the
corresponding entry in `enums.xml`, it breaks downstream metrics pipelines.
Adding `IfThisThenThat` lint guards prevents this by generating Gerrit warnings
if one side is modified without the other. Act as an expert Chromium contributor
to add these missing guards.

**Goal:** Add LINT.IfChange/ThenChange sync guards to enums in C++/Java and XML
files to enforce synchronization.

## Relevant Resources & Style Guides

- [Chromium IfThisThenThat (IFTTT) Guides](https://chromium.googlesource.com/chromium/src/+/main/docs/speed/binary_size/android_binary_size_trybot.md#IfThisThenThat)
- **Discovery Script:** [find_candidates.py](scripts/find_candidates.py)
- **Implementation Patterns:** [patterns.md](references/patterns.md) (Syntax
  rules and examples)
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
[Discovery & Batch Selection](../hub/references/discovery_and_batch_selection.md)
workflow.

### Step 3: Refactoring & Implementation

Process the candidates by handling them **one file at a time**, and applying
modifications inside each file **one enum at a time** (rather than refactoring
all files or enums at once). This ensures stability and allows for precise
verification. Refer to [patterns.md](references/patterns.md) for sync guard
syntax rules and examples before making replacements.

1. **Process Enum:** For each enum in the list:

   - **Sync Verification:** Delegate to the **`generalist`** sub-agent with this
     exact prompt:
     > "Compare the enum `<EnumName>` in `<SourcePath>` with its metadata entry
     > in `<XMLPath>`. Verify that all valid enum entries in the source have
     > corresponding `<int value="..." label="...">` entries in the XML.
     > **IMPORTANT:** Ignore/skip sentinel values like `kMaxValue`, `kCount`,
     > `COUNT`, or `NUM_ENTRIES` in the comparison; they are not expected to be
     > in the XML. Return 'SYNCED' or a concise list of missing entries that
     > need to be added to the XML. **Ensure any suggested labels reflect the
     > enum constant name and any suggested <summary> tags are concise
     > descriptions of the enum's purpose.**"
   - **Fix & Proceed:** If the `generalist` returns missing entries, add them to
     the XML file before proceeding with the guards. Only append missing entries
     to the XML. Never modify existing names or values in either file. **Do not
     shift the numeric values of an enum.** **When adding new values to the XML
     enum, check the format of the existing `<int>` entries and ensure new
     entries match that format.** **Labels added to the XML MUST be concise
     (matching the enum constant name) and any <summary> tags MUST be brief
     descriptions of what the enum tracks.**
   - **Source:**
     - **Append-Only Logging Comment:** Check if the enum's preceding comment already contains a warning about append-only histogram logging. If it does not, add the standard Chromium metric comment:
       `// These values are persisted to logs. Entries should not be renumbered and numeric values should never be reused.`
     - **Clean up legacy sync comments (CRITICAL):** Search the enum's preceding
       comments for manual synchronization reminders (e.g.,
       `// Please keep in sync with...` or
       `// Always keep this enum in sync with...`). **Remove ONLY the specific
       sentence or line** referencing the manual synchronization. Preserve all
       other parts of the comment (e.g., descriptions of what the enum
       represents, renumbering warnings, or usage instructions). If the entire
       comment block only contains a sync reminder, the whole block may be
       removed.
     - Add `// LINT.IfChange(<SourceName>)` immediately before the enum
       definition.
     - Add `// LINT.ThenChange(<XML Path>:<XmlName>)` immediately after the
       enum.
   - **XML:**
     - Add the `<!-- LINT.IfChange(<XmlName>) -->` immediately before the
       `<enum>` tag.
     - Add `<!-- LINT.ThenChange(<Source Path>:<SourceName>) -->` immediately
       after the `</enum>` tag.
   - **Handling Mismatched Names:** If the name in the Source file
     (`SourceName`) differs from the name in the XML file (`XmlName`), ensure
     the correct name is used in each respective `IfChange` and `ThenChange` tag
     as shown in the **Syntax Rules**.

2. **Iterate:** Once the enum is fully synced and verified, move to the next one
   in the list and repeat. Do not attempt to batch multiple enums in a single
   file-write operation to ensure accuracy. Once all enums have been processed,
   proceed immediately to the **Review & Validation** phase.

### Step 4: Validation

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

### Step 5: Verification

Follow the [Verification](../hub/references/verification.md) workflow.

### Step 6: Submission

Invoke the [Submission](../hub/references/submission.md) workflow. Pass the
following context variables to the workflow:

- **Skill Name:** `lint-sync`
- **Branch Name:** `lint-sync-<EnumName>`
- **Commit Hashtag:** `lint-sync`
- **Cleanup Title:** `Add missing LINT guards in <SourceFilePath>`
- **Cleanup Description:**
  `Adding IfThisThenThat lint guards to ensure synchronization between the following source code enums and their metadata representation in enums.xml: <EnumNamesList (comma-separated)>`
- **Parent Bug:** `498775295`
- **Cleaned Component:** The source file path.
- **File Count:** Number of files modified.
