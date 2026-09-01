---
name: remove-unused-object-overrides
description: Identify and safely remove unreferenced Java Object overrides (equals, hashCode, toString) in Clank to eliminate dead virtual methods from release DEX bytecode while strictly preserving equals/hashCode symmetry.
---

# Code Health: Remove Unused Object Overrides

Identify and safely remove unreferenced Java `Object` overrides (`equals`,
`hashCode`, `toString`) in Clank to eliminate dead virtual methods from release
DEX bytecode while strictly preserving method symmetry.

## Overview

Overriding `equals()`, `hashCode()`, or `toString()` on UI property holders and
data classes adds virtual methods and metadata overhead in DEX bytecode. In many
cases, these overrides were added during initial prototyping or solely for test
verifications and are never invoked in production logic. Safely removing
unreferenced overrides reduces release DEX size while maintaining object
identity semantics.

**Goal:** Clean up all unreferenced `Object` overrides in UI data holders,
properties, and model objects where equality is not needed in production code.

## Relevant Resources & Style Guides

- **Implementation Patterns:** [patterns.md](references/patterns.md)
- **Discovery Script:** [find_candidates.py](scripts/find_candidates.py)
- **Automated Review Protocol:**
  [automated_review.md](references/automated_review.md)

## Workflow

> [!IMPORTANT] **Execution Protocol:** Execute all steps sequentially one by
> one. Do not skip any step.

### Step 1: Workspace Preparation

Follow the workspace preparation steps in
[workspace_preparation.md](../hub/references/workspace_preparation.md) to ensure
a clean and updated environment.

### Step 2: Discovery & Batch Selection

Follow the
[Discovery & Batch Selection](../hub/references/discovery_and_batch_selection.md)
workflow. Present the candidates and ask for explicit approval before
proceeding.

### Step 3: Refactoring & Implementation

Process candidates case-by-case (one file at a time), applying modifications
cleanly:

1. **Safety Audit:**
   - Follow [Rule 2 in patterns.md](references/patterns.md) to audit all
     occurrences: check hashed collections (`HashMap`, `HashSet`),
     direct/indirect hashing (`Objects.hashCode`), collection lookups
     (`List.contains`, `List.indexOf`), state equality comparisons
     (`Objects.equals`), and serialization (Proto/Mojo/JSON).
   - If `equals()` is required in production (e.g. Toolbar snapshot capture
     tokens) $\\rightarrow$ **Keep BOTH `equals()` and `hashCode()`.**
   - If `equals()` is only needed for tests or not needed at all $\\rightarrow$
     **Remove BOTH `equals()` and `hashCode()`.**
2. **Apply Code Edits:**
   - Remove the unreferenced `equals()`, `hashCode()`, or `toString()` methods.
   - Clean up unused imports (such as `java.util.Objects`).
3. **Repair Unit Tests:**
   - If Mockito `verify()` calls fail due to missing `equals()`: wrap the
     argument with `refEq(expected)`.
   - If JUnit `assertEquals()` calls fail: replace with explicit field
     assertions or a local assertion helper method.

### Step 4: Validation

1. **Code Formatting:** Run `git cl format` to format changes.
2. **Mandatory Final Review:** Follow the
   [Automated Review Protocol](references/automated_review.md) to delegate a
   final review of the patch to the `generalist` sub-agent. Proceed only after
   it returns `PASS`.

### Step 5: Verification

Follow the [Verification](../hub/references/verification.md) workflow.

### Step 6: Submission

Invoke the [Submission](../hub/references/submission.md) workflow. Pass the
following context variables:

- **Skill Name:** `remove-unused-object-overrides`
- **Branch Name:** `cleanup-remove-object-overrides-[component-name]`
- **Commit Hashtag:** `Code Health`
- **Cleanup Title:** `Remove unused Object overrides in [Component/Directory]`
- **Cleanup Description:**
  `Remove unreferenced Java Object overrides (equals, hashCode, toString) in [Component/Directory] to eliminate dead virtual methods from release DEX bytecode while strictly preserving method symmetry.`
- **Parent Bug:** `544923924`
- **Bug ID:** `"none"` (to skip creating a new bug, using only the parent bug)
- **Omit Skill Attribution:** `"true"`
- **Cleaned Component:** The parent directory of the batch.
- **File Count:** Number of files cleaned up.
