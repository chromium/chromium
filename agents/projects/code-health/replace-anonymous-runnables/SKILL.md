---
name: replace-anonymous-runnables
description: Replace anonymous classes with lambdas where applicable (SAM interfaces)
---

# Code Health: Replace Anonymous Runnables

Replace Java anonymous classes implementing Single Abstract Method (SAM) interfaces with lambdas to leverage Lambda Grouping and reduce binary size.

**Goal:** Clean up all occurrences of this pattern in the codebase.

## Relevant Resources & Style Guides

- **Optimization Advice:** [optimization_advice.md](https://source.chromium.org/chromium/chromium/src/+/main:docs/speed/binary_size/optimization_advice.md;l=230)
- **Implementation Patterns:** [patterns.md](references/patterns.md)
- **Discovery Script:** [find_candidates.py](scripts/find_candidates.py)
- **Automated Review Protocol:** [automated_review.md](references/automated_review.md)

## Workflow

> [!IMPORTANT] **Execution Protocol:** Execute all steps sequentially one by
> one. Do not skip any step. Use `rg` (ripgrep) for searches.

### Step 1: Workspace Preparation

Follow the workspace preparation steps in
[workspace_preparation.md](../hub/references/workspace_preparation.md) to ensure
a clean and updated environment.

### Step 2: Discovery & Batch Selection

1. Follow the [Discovery & Batch Selection](../hub/references/discovery_and_batch_selection.md) workflow to run the scanner.
2. **Verify SAM Candidates (Subagent Verification):**
   If auditing a specific subsystem or directory, the raw scanner might return false positives (e.g. abstract classes or interfaces with multiple methods).
   To filter these out dynamically:
   - Spawn a `self` subagent.
   - Pass it the verification prompt template from [subagent_verification.md](references/subagent_verification.md) along with the list of raw candidates.
   - Use the subagent's verified list as the final batch.
3. Present the verified candidates to the user and ask for explicit approval before proceeding.

### Step 3: Refactoring & Implementation

Process the candidates by handling them **one file at a time**, and applying
modifications inside each file **one instance at a time**. This ensures
stability and allows for precise verification.

**Refactoring Guidelines:**
- Convert single-method anonymous classes to Java lambdas.
- **Warning:** Skip if the method has annotations (like `@JavascriptInterface` or `@SuppressLint`). Do not attempt to move `@SuppressLint` to the field or enclosing method to bypass this, as it may not suppress the warning on the lambda.
- **Warning:** Skip if the anonymous class body uses `this` to refer to itself.
- **Warning:** Skip if the anonymous class is in a field initializer and references blank final fields initialized in the constructor (causes compile error).
- **Warning:** Do not convert `View.OnTouchListener` to lambdas. These often trigger `ClickableViewAccessibility` lint warnings if they do not call `View#performClick()`, and converting them can break existing lint baselines.
- Refer to [patterns.md](references/patterns.md) for concrete examples.
- Ensure no functional changes are introduced.

### Step 4: Validation

1. **Code Formatting:** Run `git cl format` to format changes.
2. **Mandatory Final Review:** Follow the
   [Automated Review Protocol](references/automated_review.md) to delegate a
   final review of the patch. Proceed only after it returns `PASS`.

### Step 5: Verification

Follow the [Verification](../hub/references/verification.md) workflow.

### Step 6: Submission

Invoke the [Submission](../hub/references/submission.md) workflow. Pass the
following context variables:

- **Branch Name:** `cleanup-replace-anonymous-runnables-[component-name]`
- **Commit Hashtag:** `Code Health`
- **Cleanup Title:** `Replace anonymous classes with lambdas in [Component]`
- **Cleanup Description:**
  "Convert Java anonymous classes implementing SAM (Single Abstract Method) interfaces to lambdas. This allows R8 to perform Lambda Grouping and merge them, reducing overall DEX/binary size."
- **Parent Bug:** `541671762`
- **Bug ID:** `"none"`
- **Omit Skill Attribution:** `"true"`
- **Cleaned Component:** The parent directory of the batch.
- **File Count:** Number of files cleaned up.
