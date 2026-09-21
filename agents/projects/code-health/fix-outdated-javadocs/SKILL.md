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

## Relevant Resources & Style Guides

- **[Google Java Style Guide — § 7: Javadoc](https://google.github.io/styleguide/javaguide.html#s7-javadoc)**
- **[Chromium Java Style Guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/java/java.md)**
- **Implementation Patterns:** [patterns.md](references/patterns.md)
- **Discovery Script:** [find_candidates.py](scripts/find_candidates.py)

## Workflow

> [!IMPORTANT] **Execution Protocol:** Execute all steps sequentially. Do not
> skip any step. Use `rg` (ripgrep) for searches.

### Step 1: Workspace Preparation

Follow the workspace preparation steps in
[workspace_preparation.md](../hub/references/workspace_preparation.md).

### Step 2: Discovery & Batch Selection

Follow the
[Discovery & Batch Selection](../hub/references/discovery_and_batch_selection.md)
workflow. Present the candidates and ask for explicit approval before
proceeding.

### Step 3: Refactoring & Implementation

Process candidates one file at a time, editing comments directly. Refer to
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

### Step 4: Validation

1. **Code Formatting:** Run `git cl format` to format changes.

### Step 5: Verification

Follow the [Verification](../hub/references/verification.md) workflow.

### Step 6: Submission

Invoke the [Submission](../hub/references/submission.md) workflow with:

- **Skill Name:** `fix-outdated-javadocs`
- **Branch Name:** `cleanup-fix-outdated-javadocs-[component-name]`
- **Commit Hashtag:** `Code Health`
- **Cleanup Title:**
  `Fix outdated and mismatched Javadocs in [Component/Directory]`
- **Cleanup Description:**
  `Audit and fix outdated or mismatched Javadoc comments in [Component/Directory] per the Java style guide.`
- **Parent Bug:** `564124183`
- **Bug ID:** `"none"`
- **Omit Skill Attribution:** `"true"`
- **Cleaned Component:** The parent directory of the batch.
- **File Count:** Number of files cleaned up.
