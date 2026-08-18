---
name: cr-git-fix-conflicts
description: >-
  Resolves git merge conflicts. Files should show in "git status" and contain
  conflict markers ("<<<<").
---

# Fix git conflicts

Resolve the merge conflicts in the working tree.

Conflicted files appear in `git status` and contain conflict markers (`<<<<`).

Do NOT compile or validate your changes using any tools.

Run `git add <path>` on each file once its conflicts are successfully resolved,
staging only the files you have fully resolved by their explicit paths. Do NOT
use broad staging forms such as `git add .` or `git add -A`, so that unrelated
or unresolved changes cannot be staged. Do not run any other git commands (for
example, do not run `git rebase --continue`).
