# GitHub Copilot Integration in Chromium

This directory provides instructions and prompts for integrating GitHub Copilot
with the chromium codebase.

This directory is currently in a prototyping state and may be removed in the
future. As we add support for multiple coding IDE/agents, we will likely pull
common prompts and instructions into a central directory with stubs for bespoke
IDE/agent integration. Please check with your organization before using GitHub
Copilot.

## Where is copilot-instructions.md?
[`copilot-intructions.md`](../copilot-instructions.md) is typically a single
instruction file that contains default instructions for a workspace. These
instructions are automatically included in every chat request.

Until the prompt in `copilot-intructions.md` is generally agreed upon for the
chromium repo, this file is intentionally excluded from the repo, and added to
the [.gitignore](../.gitignore) for your customization.

For generating your own `copilot-intructions.md`, type
`/create_copilot_instructions` in GitHub Copilot to get started.

## Code Layout
- [.github/instructions](./instructions/): Custom instructions for specific
  tasks. For example, you can create instruction files for different programming
  languages, frameworks, or project types. You can attach individual prompt
  files to a chat request, or you can configure them to be automatically
  included for specific files or folders with `applyTo` syntax.
- [.github/prompts](./prompts/): Prompt files can be easily triggered from chat
  with `/` and allow you to craft complete prompts in Markdown files.
  Unlike custom instructions that supplement your chat queries prompts, prompt
  files are standalone prompts that you can store within your workspace and
  share with others. With prompt files, you can create reusable templates for
  common tasks, store domain expertise in the codebase, and standardize AI
  interactions across your team.
- [.github/resources](./resources/): Prompt files that are resources for use by
  other prompts and instructions.

## User Specific Prompts
Users can create their own prompts or instructions that match the regex
`.github/**/user_.md` which is captured in the [.gitignore](../.gitignore).

## Contributing Guidelines
Use `/git_commit_ghc`

- [.github/instructions](./instructions/): Instructions that are automatically
  picked up using `applyTo` syntax will have a much higher review bar then those
  without it.
- [.github/prompts](./prompts/): All prompts should specify a `mode` and
  `description`.
- [.github/resources](./resources/): All prompt resources should have an active
  reference or usecase a file in `instructions` or `prompts`, and should be
  cleaned up if their references are modified or removed.
