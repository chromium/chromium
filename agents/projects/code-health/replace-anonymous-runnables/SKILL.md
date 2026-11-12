---
name: replace-anonymous-runnables
description: Replace anonymous classes with lambdas where applicable (SAM interfaces)
---

# Code Health: Replace Anonymous Runnables

Replace Java anonymous classes implementing Single Abstract Method (SAM)
interfaces with lambdas to leverage Lambda Grouping and reduce binary size (see
[optimization_advice.md](https://source.chromium.org/chromium/chromium/src/+/main:docs/speed/binary_size/optimization_advice.md;l=230)).

**Goal:** Clean up all occurrences of this pattern in the codebase.

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
workflow. Before reporting the batch and proceeding to the next step, verify raw
directory/subsystem candidates against false positives (e.g., abstract classes
or multi-method interfaces) using a **`self`** subagent (`invoke_subagent`,
`TypeName="self"`, `Role="SAM Candidate Verifier"`) with
[subagent_verification.md](references/subagent_verification.md) (or, if
subagents are unavailable, follow those instructions inline). Use the subagent's
verified list as the final batch; if none remain valid, re-run discovery with
`--exclude`.

### Step 3: Refactoring & Implementation

Process the candidates by handling them **one file at a time**, and applying
modifications inside each file **one instance at a time**. This ensures
stability and allows for precise verification. Process all files and instances
sequentially within the same run, without pausing for user confirmation between
edits, then proceed directly to Step 4.

**Refactoring Guidelines:**

- Convert single-method anonymous classes to Java lambdas.
- **Warning:** Skip if the method has annotations (like `@JavascriptInterface`
  or `@SuppressLint`). Do not attempt to move `@SuppressLint` to the field or
  enclosing method to bypass this, as it may not suppress the warning on the
  lambda.
- **Warning:** Skip if the anonymous class body uses `this` to refer to itself.
- **Warning:** Skip if the anonymous class is in a field initializer and
  references blank final fields initialized in the constructor (causes compile
  error).
- **Warning:** Do not convert `View.OnTouchListener` to lambdas. These often
  trigger `ClickableViewAccessibility` lint warnings if they do not call
  `View#performClick()`, and converting them can break existing lint baselines.
- Refer to [patterns.md](references/patterns.md) for concrete examples.
- Ensure no functional changes are introduced.

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
  --skill replace-anonymous-runnables \
  --title "Replace anonymous classes with lambdas in <Component>" \
  --description - \
  --out-commit ../commit <<'EOF'
Convert Java anonymous classes implementing SAM (Single Abstract Method)
interfaces to lambdas in <Component>. This allows R8 to perform Lambda
Grouping and merge them, reducing overall DEX/binary size.
EOF
```

#### 2. Validate Commit File

```bash
python3 agents/projects/code-health/hub/scripts/prepare_commit_message.py \
  --skill replace-anonymous-runnables \
  --validate ../commit
```

If validation fails, fix the commit title or description and re-run Steps 1–2
until validation exits 0.

#### 3. Submit CL

Invoke the [Submission](../hub/references/submission.md) workflow, passing:

- **Branch Name:** `cleanup-replace-anonymous-runnables-<component-name>`
- **Commit File:** `../commit`
