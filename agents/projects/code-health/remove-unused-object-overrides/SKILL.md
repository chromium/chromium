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

Process candidates case-by-case (one file at a time), applying modifications
cleanly. Process all files sequentially within the same run, without pausing for
user confirmation between edits, then proceed directly to Step 4:

1. **Safety Audit:**
   - Follow [Rule 2 in patterns.md](references/patterns.md) to audit all
     occurrences: check hashed collections (`HashMap`, `HashSet`),
     direct/indirect hashing (`Objects.hashCode`), collection lookups
     (`List.contains`, `List.indexOf`), state equality comparisons
     (`Objects.equals`), and serialization (Proto/Mojo/JSON).
   - If `equals()` is required in production (e.g. Toolbar snapshot capture
     tokens) -> **Keep BOTH `equals()` and `hashCode()`.**
   - If `equals()` is only needed for tests or not needed at all -> **Remove
     BOTH `equals()` and `hashCode()`.**
2. **Apply Code Edits:**
   - Remove the unreferenced `equals()`, `hashCode()`, or `toString()` methods.
   - Clean up unused imports (such as `java.util.Objects`).
3. **Repair Unit Tests:**
   - If Mockito `verify()` calls fail due to missing `equals()`: wrap the
     argument with `refEq(expected)`.
   - If JUnit `assertEquals()` calls fail: replace with explicit field
     assertions or a local assertion helper method.

### Step 4: Formatting & Automated Review

1. **Format & Lint:** Run
   `python3 agents/projects/code-health/hub/scripts/format_and_lint.py` (must
   exit 0).
2. **Automated Review:** Follow
   [Automated Review](../hub/references/automated_review.md), passing
   [review_rules.md](references/review_rules.md) as
   `<SKILL_SPECIFIC_CLEANUP_RULES>`.

### Step 5: Verification

Follow the [Verification](../hub/references/verification.md) workflow.

### Step 6: Submission

#### 1. Prepare Commit Message

Ensure the full `[Code Health][LSC] <Title>` subject is at most 72 characters
(use a short component name or shorten the title if needed), and generate
`../commit`:

```bash
python3 agents/projects/code-health/hub/scripts/prepare_commit_message.py \
  --skill remove-unused-object-overrides \
  --title "Remove unused Object overrides in <Component>" \
  --description - \
  --out-commit ../commit <<'EOF'
Remove unreferenced Java Object overrides (equals, hashCode, toString) in
<Component/Directory> to eliminate dead virtual methods from release DEX
bytecode while strictly preserving method symmetry.
EOF
```

#### 2. Validate Commit File

```bash
python3 agents/projects/code-health/hub/scripts/prepare_commit_message.py \
  --skill remove-unused-object-overrides \
  --validate ../commit
```

If validation fails, fix the commit title or description and re-run Steps 1–2
until validation exits 0.

#### 3. Submit CL

Invoke the [Submission](../hub/references/submission.md) workflow, passing:

- **Branch Name:** `cleanup-remove-object-overrides-<component-name>`
- **Commit File:** `../commit`
