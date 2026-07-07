# Workspace Preparation

Before making any modifications or running discovery scripts, ensure a clean and
isolated environment.

1. **Handle Local Changes:** Run
   `git status --porcelain -uno --ignore-submodules`. If there is any output,
   run `git stash` and inform the user: "I noticed uncommitted changes; I've
   stashed them (`git stash`) to ensure a clean environment."
2. **Switch and Update:** Always start fresh from `main`:
   `git checkout main && git pull origin main --rebase > /dev/null && gclient sync -D > /dev/null 2>&1`
3. **Check for unmerged local commits:** Run `git log origin/main..HEAD`. If
   there is any output, stop and inform the user, as we do not want to carry
   these over to a new branch.
