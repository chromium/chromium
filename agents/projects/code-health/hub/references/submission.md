# Shared Code Health Submission Pipeline

**CRITICAL:** Do not execute this file unless explicitly invoked by a skill.

When invoked, the calling skill provides these context variables:

- `Branch Name`
- `Commit File` (e.g., `../commit`)
- `Upload Flags` (optional, e.g., `--reviewers ... --cc ...`)
- `Reviewer Lookup` (optional; command that suggests reviewers. If omitted, use
  `python3 agents/projects/code-health/hub/scripts/find_owners.py --files <modified files>`.
  If the caller passes `none`, the caller already made its reviewer decision
  before invoking this file: do not run any command for the lookup (never
  execute `none` in bash), skip the lookup in the "No" branch and Step 5, and
  report the caller's pre-upload reviewer decision in the Step 7 summary)

First, prompt the user using `ask_question` with the question: *"Create a
branch, commit these changes, and upload a CL to Gerrit?"* and choices "Yes,
submit this cleanup" and "No, keep local changes uncommitted".

- **If the user chooses "No":** Run `<Reviewer Lookup>` (unless it is `none`;
  then run nothing and use the caller's pre-upload reviewer decision). Print a
  summary of the changes that includes the suggested owners and resolved
  immediate `OWNERS` (or, when `Reviewer Lookup` is `none`, the caller's
  pre-upload reviewer decision), print a success message, leave the changes
  uncommitted in the local working directory, and end the workflow here.

- **If the user chooses "Yes":** The following steps must be performed
  sequentially, one by one, in the exact order specified, without skipping or
  jumping:

  1. **Branch Creation**: Run `git new-branch <Branch Name>`.
  2. **Commit**:
     - Stage only the changed files for this cleanup using `git add <files>` (do
       not run `git add -A` or `git add .`, so `<Commit File>` is never staged).
     - Display the contents of `<Commit File>` to the user and commit using
       `git commit -F <Commit File>`.
  3. **Upload**:
     - First, run `git pull origin main --rebase && gclient sync -D` without
       hiding its output. If it fails, stop and surface/resolve the conflicts or
       sync errors instead of ignoring them.
     - Then run `git cl upload --force -a -d`, appending any provided
       `Upload Flags` (e.g., `--reviewers ... --cc ...`). Never pass
       `--bypass-hooks`.
  4. **Address Presubmit Feedback:** If the upload fails due to presubmit
     errors, the output must be analyzed, issues in the code resolved, and the
     upload re-attempted using the exact same `git cl upload` command from Step
     3, including any `Upload Flags`. These checks must not be bypassed.
  5. **Find Owners**: Once upload succeeds, run `<Reviewer Lookup>` before
     leaving the branch. If `Reviewer Lookup` is `none`, skip this step and run
     no command.
  6. **Return to main**: Run `git checkout main` so the workspace is back on
     `main` for the next run while keeping the uploaded branch intact.
  7. **Summary**: Print a summary including the CL link, the Bug ID(s) from
     `<Commit File>`, the commit message, the number of modified files, and the
     suggested owners and `OWNERS` notes (or the recommended reviewer) from Step
     5\. If `Reviewer Lookup` is `none`, report the caller's pre-upload reviewer
     decision instead.
