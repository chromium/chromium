---
name: apply-fix-from-crbug-with-diff
description: Automates applying a bug fix from a bug report containing a diff and CL description. Supports processing multiple bugs in parallel using subagents.
---

# Apply Bug Fix Skill

This skill provides a structured workflow for applying bug fixes that contain a
diff and a suggested CL description within the bug comments.

## Workflow Instructions

When asked to apply one or more bug fixes, strictly follow these steps:

### 1. Parallelization

- If you are asked to process multiple bugs, you MUST parallelize the workload.
- Use the `invoke_subagent` tool to spawn a separate `self` subagent for each
  bug.
- When invoking subagents, you MUST set the `Workspace` parameter to `share` so
  that each subagent gets its own isolated git worktree. This prevents branch
  checkout conflicts when parallelizing work in the same repository.
- Prepend `/goal` to the Prompt given to the subagent so that it runs
  continuously until its bug fix is complete.
- Wait for all subagents to report completion.

### 2. Branching Strategy

- You MUST create a new branch starting directly from `origin/main`.
- Command format: `git checkout -b fix-<bug_id> origin/main`
- After checking out the branch, you MUST run `gclient sync -D --force` to
  ensure all dependencies are correctly synchronized for the target branch.

### 3. Extracting Bug Details

- Retrieve the bug details (e.g., via the Buganizer CLI).
- Find the diff file or diff text in the comments.
- Find the suggested CL (Commit List) description in the comments.
- **Information Scrubbing**: Never mention any code names, internal tools, or
  automated systems used to generate the bug or fix. If the bug comments mention
  any such internal tooling, you must silently ignore it. Do not include it in
  the CL description, commit messages, or your chat responses to the user.

### 4. Applying the Fix

- Save the extracted diff to a local file (e.g., `bug_fix.patch`).
- Apply the patch using `git apply bug_fix.patch`.
- Run the required formatting tools for the repository (e.g., `git cl format`).
- Run necessary builds and tests (e.g., `autoninja`, unit tests) to verify the
  patch compiles and passes.

### 5. Committing and Uploading

- Before committing, ensure you delete the `bug_fix.patch` file or any other
  temporary files.

- Stage only the source files modified by the patch (e.g., using `git add -u`).
  DO NOT commit the patch file itself.

- Commit the changes using the extracted CL description.

- Ensure the commit message includes the bug tag in the correct format (e.g.,
  `Bug: <bug_id>`) at the bottom.

- Upload the fix using `git cl upload`. Run `git cl presubmit` before uploading
  to ensure all code quality checks pass.
