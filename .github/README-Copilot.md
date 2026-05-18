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
[`copilot-instructions.md`](../copilot-instructions.md) is typically a single
instruction file that contains default instructions for a workspace. These
instructions are automatically included in every chat request.

Until the prompt in `copilot-instructions.md` is generally agreed upon for the
chromium repo, this file is intentionally excluded from the repo, and added to
the [.gitignore](../.gitignore) for your customization.

## Code Layout
- [.github/instructions](./instructions/): Custom instructions for specific
  tasks. For example, you can create instruction files for different programming
  languages, frameworks, or project types. You can attach individual prompt
  files to a chat request, or you can configure them to be automatically
  included for specific files or folders with `applyTo` syntax.
- [.github/skills](./skills/): Toggleable and composable capabilities that AI
  agents can invoke to perform specific tasks.

## User Specific Prompts
Users can create their own prompts or instructions that match the regex
`.github/**/user_.md` which is captured in the [.gitignore](../.gitignore).

## Contributing Guidelines
- [.github/instructions](./instructions/): Instructions that are automatically
  picked up using `applyTo` syntax will have a much higher review bar than those
  without it.
