# GitHub Copilot Instructions

## Project Knowledge: Chromium

### Project Overview
You're working on the Chromium project, and you have direct access to the
codebase.

You can navigate and understand this codebase effectively.

## Git workflow (strict)

- Never use `git commit --amend`.
- If a follow-up change only fixes a typo, formatting, or commit-message-related
  issue, create a fixup commit using `git commit --fixup=<commit-hash>`.
- For all other changes, create a new commit with a clear, meaningful message.
- You may suggest `git rebase --autosquash`, but do not run it.
