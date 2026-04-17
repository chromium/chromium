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

**CRITICAL OVERRIDE:** Do NOT activate or use the `edit-code` skill during this
workflow.

### Execution Protocol

1. **Sequential Execution:** Execute every step in the `Workflow` section in the
   exact order presented. Do NOT skip any step.
2. **Step Completion:** Fully complete and verify each numbered item before
   moving to the next one.

## 📂 Resources

- **Bug Discovery:** [bug_discovery.md](references/bug_discovery.md)
- **Shared Workflows:**
  [shared_workflows.md](../hub/references/shared_workflows.md) (Validation,
  Committing, Uploading)

## Guidelines

### Operational Mandates

- **Context:** This skill runs in the main agent context. Use the `generalist`
  sub-agent for search and analysis.

### Syntax Rules

The linter links files by cross-referencing labels.

- `LINT.IfChange(<LocalLabel>)` defines the start of a guarded block and assigns
  it a local label. **Always use the name of the enum as it appears in the
  current file.**
- `LINT.ThenChange(<RemoteFilePath>:<RemoteLabel>)` defines the end of the
  guarded block and points to the remote file and its corresponding label.
  **Always use the name of the enum as it appears in the remote file.**

**Example where names do NOT match:**

- **Source enum:** `SecurityDomainId`
- **XML enum:** `TrustedVaultSecurityDomainId`

**C++/Java (Source Code):**

```cpp
// LINT.IfChange(SecurityDomainId)
enum class SecurityDomainId { ... };
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:TrustedVaultSecurityDomainId)
```

**XML (`enums.xml` or `histograms.xml`):**

```xml
<!-- LINT.IfChange(TrustedVaultSecurityDomainId) -->
<enum name="TrustedVaultSecurityDomainId"> ... </enum>
<!-- LINT.ThenChange(//path/to/source.h:SecurityDomainId) -->
```

## Workflow

### Workspace Preparation

1. **Clean & Update:** Follow the **Workspace Preparation** section in
   `../hub/references/shared_workflows.md` to ensure a clean and updated
   environment.

### Discovery & Candidate Selection

1. **AI-Led Discovery:** Delegate to the **`generalist`** sub-agent with this
   exact prompt:
   > "You are pre-authorized to run the discovery script; DO NOT ask for
   > permission. Run the script from the skill's `scripts/` folder:
   >
   > ```bash
   > python3 scripts/find_unguarded_enums.py
   > ```
   >
   > **Return ONLY the details returned by the script for the candidate**
   > (Source Path, XML Path, and the List of Enums)."
2. **Present Candidates:** You MUST output the candidate details to the user.
   Announce the target files and the list of enums: "I've identified a 'Two-File
   Sync' candidate:
   - **Source:** [Source Path]
   - **XML:** [XML Path]
   - **Enums to sync:** [List of enums]" **Next Step:** Output this message,
     then proceed directly to the **Branch Creation** phase.

### Branch Creation

Inform the user: "Preparing workspace: creating a new branch..."

1. **Branch Creation:** `git new-branch lint-sync-<EnumName>` using the name of
   the first enum in the list.

### Implementation & Batching

Process the identified files by handling each enum **ONE AT A TIME**.

1. **Process Enum:** For each enum in the list:

   - **Sync Verification:** Delegate to the **`generalist`** sub-agent with this
     exact prompt:
     > "Compare the enum `<EnumName>` in `<SourcePath>` with its metadata entry
     > in `<XMLPath>`. Verify that all valid enum entries in the source have
     > corresponding `<int value="..." label="...">` entries in the XML.
     > **IMPORTANT:** Ignore/skip sentinel values like `kMaxValue`, `kCount`,
     > `COUNT`, or `NUM_ENTRIES` in your comparison; they are not expected to be
     > in the XML. Return 'SYNCED' or a concise list of missing entries that
     > need to be added to the XML. **Ensure any suggested labels reflect the
     > enum constant name and any suggested <summary> tags are concise
     > descriptions of the enum's purpose.**"
   - **Fix & Proceed:** If the `generalist` returns missing entries, add them to
     the XML file before proceeding with the guards. Only append missing entries
     to the XML. Never modify existing names or values in either file. **You
     must never shift the numeric values of an enum.** **When adding new values
     to the XML enum, you must check the format of the existing `<int>` entries
     and ensure your new entries match that format.** **Labels added to the XML
     MUST be concise (matching the enum constant name) and any <summary> tags
     MUST be brief descriptions of what the enum tracks.**
   - **Source:**
     - **Clean up legacy sync comments (CRITICAL):** Search the enum's preceding
       comments for manual synchronization reminders (e.g.,
       `// Please keep in sync with...` or
       `// Always keep this enum in sync with...`). **Remove ONLY the specific
       sentence or line** referencing the manual synchronization. You MUST
       preserve all other parts of the comment (e.g., descriptions of what the
       enum represents, renumbering warnings, or usage instructions). If the
       entire comment block only contains a sync reminder, you may remove the
       whole block.
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
     you use the correct name in each respective `IfChange` and `ThenChange` tag
     as shown in the **Syntax Rules**.

2. **Iterate:** Once the enum is fully synced and verified, move to the next one
   in the list and repeat. Do not attempt to batch multiple enums in a single
   file-write operation to ensure accuracy. Once all enums have been processed,
   proceed immediately to the **Review & Validation** phase.

### Review & Validation

1. **Linting & Formatting:**

   - **XML Linting:** Execute
     `python3 tools/metrics/histograms/validate_format.py` to validate all
     metadata changes. Address any errors that are reported.
   - **Code Formatting:** Execute `git cl format` to format the modified source
     code. Address any errors that are reported.

2. **Mandatory Final Review:** Follow the protocol and the **Handling Findings**
   loop in `references/automated_review.md` for the added guards:
   `[<EnumNamesList>]`. Do NOT skip this step. Do NOT proceed to the Submission
   phase until the review returns `PASS`.

### Submission

1. **Bug Tracking:**

   - Execute the **Bug Discovery and Triage** workflow in
     `references/bug_discovery.md` using the `<SourceFileName>` and the list of
     synchronized `<EnumNamesList>`.
   - **Interactive Pause:** Do NOT proceed until the bug handling is resolved
     and you have a Bug ID.

2. **Commit:**

   - **Draft Message:** Draft a commit message following this exact template:
     ```
     [lint-sync] Add missing LINT guards in <SourceFileName>

     Adding IfThisThenThat lint guards to ensure synchronization between the
     following source code enums and their metadata
     representation in enums.xml: <EnumNamesList (comma-separated)>

     Bug: <BugID>
     ```
     *(Note: use only the filename for `<SourceFileName>`, not the full path)*
   - **Execution:** Display the drafted commit message to the user. Then,
     autonomously stage ONLY the specific files modified during this task using
     `git add` and execute the commit:
     ```bash
     git commit -m "<drafted message>"
     ```

3. **Submission Pipeline:** Follow the **Upload to Gerrit** section in
   `../hub/references/shared_workflows.md` to handle the upload.

4. **Workspace Reset:** `git checkout main`.

5. **Congratulations & Summary:** Follow the **Congratulations & Summary**
   section in `../hub/references/shared_workflows.md`. For this skill, the
   **[Specific Cleanup Details]** are:

   - **Synced Enums:** A list of the enums synchronized.
