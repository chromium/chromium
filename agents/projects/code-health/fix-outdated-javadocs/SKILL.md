---
name: fix-outdated-javadocs
description: Audit and fix outdated or mismatched Javadoc comments by updating parameter names, adding missing @param tags, removing stale tags, converting return-only tags, and replacing pipe syntax per the Java style guide.
---

# Code Health: Fix Outdated Javadocs

Audit and fix outdated or mismatched Javadoc comments in Java files to align
with current method signatures and the Google Java Style Guide.

## Overview

Method signatures evolve as parameters are added, renamed, or removed, leaving
Javadocs out of sync. This cleanup fixes stale/missing `@param` tags, converts
return-only comments into summary fragments (`"Returns ..."`), and replaces C++
pipe syntax (`|param|`) with `{@code param}`.

**Goal:** Clean up outdated Javadoc comments in first-party Java code.

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
[Discovery & Batch Selection](../hub/references/discovery_and_batch_selection.md)
workflow.

### Step 3: Refactoring & Implementation

Process candidates one file at a time, editing comments directly. Process all
files sequentially within the same run, without pausing for user confirmation
between edits, then proceed directly to Step 4. Refer to
[patterns.md](references/patterns.md) for syntax rules and examples:

1. **Parameter Alignment:**
   - **Missing parameters:** Add `@param <name> <desc>` (never leave empty
     descriptions).
   - **Stale parameters:** Remove `@param` tags for parameters no longer in the
     signature.
   - **Renamed / Typos:** Update `@param` names to match signature casing and
     identifiers.
2. **Syntax & Formatting:**
   - **Return-only:** Convert `/** @return foo */` to summary fragment
     `/** Returns foo. */`.
   - **Pipe syntax:** Replace `|param|` with `{@code param}`.
   - **Inline `@param`:** Replace invalid `{@param foo}` in sentences with
     `{@code foo}`.
   - **Tag ordering:** Ensure order is `@param`, `@return`, `@throws`,
     `@deprecated` with 4-space continuation indent.
3. **Safety:** Documentation-only changes. Do NOT modify method signatures or
   code logic. Preserve existing summary text and HTML formatting. Only audit
   and update methods that already have a Javadoc block; do NOT generate
   brand-new Javadoc comments for undocumented methods, private methods, or
   trivial getters/setters.

### Step 4: Formatting & Automated Review

1. **Format & Lint:** Run
   `python3 agents/projects/code-health/hub/scripts/format_and_lint.py` (must
   exit 0).
2. **Automated Review:** Skipped (documentation-only changes); proceed directly
   to Step 5.

### Step 5: Verification

Follow the [Verification](../hub/references/verification.md) workflow.

### Step 6: Submission

#### 1. Prepare Commit Message

Ensure the full `[Code Health][LSC] <Title>` subject is at most 72 characters
(use a short component name or shorten the title if needed), and generate
`../commit`:

```bash
python3 agents/projects/code-health/hub/scripts/prepare_commit_message.py \
  --skill fix-outdated-javadocs \
  --title "Fix outdated Javadocs in <Component>" \
  --description - \
  --out-commit ../commit <<'EOF'
Audit and fix outdated or mismatched Javadoc comments in
<Component/Directory> per the Java style guide.
EOF
```

#### 2. Validate Commit File

```bash
python3 agents/projects/code-health/hub/scripts/prepare_commit_message.py \
  --skill fix-outdated-javadocs \
  --validate ../commit
```

If validation fails, fix the commit title or description and re-run Steps 1–2
until validation exits 0.

#### 3. Submit CL

Invoke the [Submission](../hub/references/submission.md) workflow, passing:

- **Branch Name:** `cleanup-fix-outdated-javadocs-<component-name>`
- **Commit File:** `../commit`
