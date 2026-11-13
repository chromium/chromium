# Chromium Gerrit Code Review

Unless the caller allows changes, provides a review checkout, or confirms first,
treat the state as read-only (do not edit files, apply patches, build, or run
tests).

Unless the caller asks you to upload Gerrit drafts, report findings and
suggested code review comments only in chat.

## Fetch Gerrit Change Context (Extends Step 1)

Fetch the change details, relation chain, review threads, and automated checks
with the [`gerrit-cli` skill](/agents/shared/skills/gerrit-cli/SKILL.md). If the
change is not applied locally and checkout edits are allowed, run
`git cl patch`.

## 6. (Optional) Prototype Architectural Changes

If better architectures or simplifications exist, you can prototype, build, and
test them locally. If the caller has not allowed checkout edits and builds, ask
first. Report any findings or verified suggestions to the caller.

## 7. (Optional) Review After Changes

Verify whether the author resolved prior review comments. Check the new patchset
diff for new issues, and report updated findings to the caller. Unless the
caller asks you to upload Gerrit drafts, keep comments in chat.

- **Apply the new patchset:** In a mutable or dedicated review checkout, follow
  the
  [new patchset instructions](/agents/skills/code-review/references/review-new-gerrit-patchset.md)
  to apply it locally.
- **Spawn a reviewer subagent (recommended for non-trivial changes):** For
  changes with any amount of complexity, or if there have been more than 2
  trivial follow-ups, spawn a fresh `code_reviewer` subagent so prior turns do
  not anchor the pass. Pass the relevant change context, and let the subagent
  explore on its own.
