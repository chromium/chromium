---
name: git-cl-helper
description: Use this skill for the Chromium Gerrit CL workflow with Git - committing changes, uploading CLs, triggering try jobs, and polling presubmit results.
---

# Git CL Helper Skill

This skill assists with the full Chromium Gerrit CL workflow with Git. It
enables creating commits, uploading CLs with various options, starting try jobs,
and polling presubmit results to verify local changes.

## Prerequisites

- **Git**: This skill uses [Git](https://git-scm.com/) to manage changes. Ensure
  that Git is properly configured for your development environment.
- **depot_tools**: The required tools (`git cl`, `bb`, and `vpython3`) are part
  of
  [depot_tools](https://www.chromium.org/developers/how-tos/install-depot-tools/).
  If any tools are missing, ask the user to configure depot_tools in the `PATH`.

## Usage

### Creating a Commit

Before uploading a CL, you must commit your local changes. Follow these steps to
create a commit:

1. **Stage changes**: Use `git add <files>` to stage the files you want to
   include in the commit.
   - Use `git add -u` to stage all modified files that are already tracked.
2. **Create commit**: Use `git commit` to create a commit.
   - It is recommended to use `git commit -m "Commit message"` to specify the
     message directly.
   - The first line should be a short summary (up to 72 characters).

Example:

```bash
git add chrome/browser/ui/views/tabs/tab.cc
git commit -m "Fix layout issue in tab"
```

### Uploading a CL

To upload a CL, use `git cl upload`. You should choose appropriate options based
on your task requirements. Here are some common and useful options to make the
process more granular and flexible:

- **Basic upload**: `git cl upload` Use this to upload the current branch as a
  CL.

- **CQ dry run**: `git cl upload -d` or `git cl upload --cq-dry-run` Use this to
  upload the CL and immediately trigger a CQ dry run. If you want to upload the
  CL *without* triggering any trybots, simply use `git cl upload` without these
  flags.

- **Resolve presubmit failures**: `git cl upload` may fail if there are
  presubmit check errors. Resolve these before attempting to upload again.
  Follow instructions for how to fix the patch if there are errors.

  For formatting errors, run `git cl format` to automatically apply Chromium
  style formatting to your changes, then amend your CL.

- **Skip hooks**: `git cl upload --bypass-hooks` Use this to bypass pre-upload
  hooks. Use this only when necessary (e.g., when hooks are failing due to known
  issues and you need to upload anyway).

- **Set title**: `git cl upload -t "Title of the CL"` Use this to set the title
  of the CL directly from the command line. (Note: `-m` is deprecated in favor
  of `-t` or `--title`).

- **Set description**:
  `git cl upload --commit-description="Detailed description"` Use this to set
  the full description of the CL. You can also use `--edit-description` to open
  an editor and modify the description interactively before uploading.

- **Squash commits**: `git cl upload --squash` Use this to squash multiple local
  commits into a single CL.

You should select the minimal set of flags necessary for your task. For a full
list of options, you can run `git cl upload --help`.

### Starting Try Jobs

After uploading a CL, you can manually start try jobs to verify your changes.
This is useful if you need to run specific bots or if they are not started
automatically when uploading the CL.

- **Start all default dry run try jobs**: `git cl try` Use this to trigger the
  default set of try jobs for the CL. This is equivalent to triggering a CQ Dry
  Run.

- **Start specific try job**: `git cl try -b <builder_name>` Use this to start a
  try job on a specific builder. You must specify the builder name. You can use
  this flag multiple times to trigger multiple builders.

- **Specify bucket**: `git cl try -B <bucket_name> -b <builder_name>` Use this
  to specify the bucket if it is not the default (usually `luci.chromium.try`).

Example:

```bash
git cl try -b linux-rel -b win-rel
```

### Polling Status

To poll the presubmit status of a CL, use the provided Python script
`agents/skills/git-cl-helper/scripts/git_cl_helper.py`. Example usage:
`vpython3 agents/skills/git-cl-helper/scripts/git_cl_helper.py poll --gerrit_url <GERRIT_URL>`

The `GERRIT_URL` can be obtained either from the upload step above, or by
running the `git cl web --print-only` command locally. It needs to be a valid
URL such as `https://crrev.com/c/1234567` or
`https://chromium-review.googlesource.com/1234567`.

This will print the status (e.g., `STATUS: passed`, `STATUS: failed`,
`STATUS: running`) and detailed error logs if any presubmit build failed,
fetched via `bb get`.
