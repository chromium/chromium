# Workspace Preparation

Before making any modifications or running discovery scripts, ensure a clean and
isolated environment.

Run:

```bash
python3 agents/projects/code-health/hub/scripts/prepare_workspace.py --stash
```

The script stashes tracked local changes (under `code-health-workspace-prep`),
switches to `main`, checks for unmerged commits on `main`, pulls `origin/main`
with rebase, and runs `gclient sync -D`.

- If the script outputs `(stashed local changes)`, inform the user: "I noticed
  uncommitted changes; I've stashed them (`git stash`,
  `code-health-workspace-prep`) to ensure a clean environment."
- If the script exits non-zero (for example due to unmerged local commits on
  `main` or sync errors), stop and report the error to the user. Do not continue
  on a dirty or diverged tree.
