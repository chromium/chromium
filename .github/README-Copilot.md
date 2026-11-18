# GitHub Copilot Integration in Chromium

This directory provides instructions and prompts for integrating GitHub Copilot
with the chromium codebase.

This directory is currently in a prototyping state and may be removed in the
future. As we add support for multiple coding IDE/agents, we will likely pull
common prompts and instructions into a central directory with stubs for bespoke
IDE/agent integration. Please check with your organization before using GitHub
Copilot.

Note: This README file is intentionally NOT named /.github/README.md to avoid
replacing the root level [README.md](../README.md) on
https://github.com/chromium/chromium. See
[github readme documentation](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-readmes)

## Where is copilot-instructions.md?
[`copilot-instructions.md`](copilot-instructions.md) is typically a single
instruction file that contains default instructions for a workspace. These
instructions are automatically included in every chat request.

Until the prompt in `copilot-instructions.md` is generally agreed upon for the
chromium repo, this file is intentionally excluded from the repo, and added to
the [.gitignore](.gitignore) for your customization.

## Code Layout
- [.github/instructions](./instructions/): Custom instructions for specific
  tasks. For example, you can create instruction files for different programming
  languages, frameworks, or project types. You can attach individual prompt
  files to a chat request, or you can configure them to be automatically
  included for specific files or folders with `applyTo` syntax.
- [.github/skills](./skills/): Toggleable and composable capabilities that AI
  agents can invoke to perform specific tasks.

## Shared and personal skills

[.github/skills](./skills/) is for shared, tracked Copilot project skills such as
[style-guide](./skills/style-guide/SKILL.md).

Repository-local **personal** skills should not be placed under
`.github/skills/`. Instead, place them under `.agents/skills/`.
These will not be tracked by git.

## Worktree command approval

The [shell command hook](./hooks/approve-worktree-creation.json) requests approval
for `git worktree add` on all platforms, including Git options such as
`git -C path worktree add`.
Creating a Chromium worktree can take time and use substantial disk space.
To disable the hook, open `/config`, go to **Capabilities > Hooks**, and disable
this hook.

For additional buildable working directories, use depot_tools'
`gclient-new-workdir.py`, as described in Chromium's
[Managing Multiple Working Directories][multiple-working-directories] guide.
The helper handles the gclient workspace and its dependency repositories, and
uses copy-on-write where supported.

[multiple-working-directories]: https://www.chromium.org/developers/how-tos/get-the-code/multiple-working-directories/

## Documentation

Official documentation:
- Custom instructions:
  <https://docs.github.com/en/copilot/how-tos/copilot-cli/customize-copilot/add-custom-instructions>
- Agent skills:
  <https://docs.github.com/en/copilot/how-tos/copilot-cli/customize-copilot/add-skills>
