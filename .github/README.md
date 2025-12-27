# GitHub Copilot Integration in Chromium

This directory provides instructions and prompts for integrating GitHub Copilot
with the chromium codebase.

This directory is currently in a prototyping state and may be removed in the
future. As we add support for multiple coding IDE/agents, we will likely pull
common prompts and instructions into a central directory with stubs for bespoke
IDE/agent integration. Please check with your organization before using GitHub
Copilot.

### About copilot-instructions.md
`copilot-instructions.md` is a user-local configuration file that defines default
GitHub Copilot instructions for a workspace. Its contents are automatically
included in Copilot chat requests when present.

This file is intentionally not included in the Chromium repository. The prompt
has not yet been standardized across the project, so each contributor is
expected to create and maintain their own local version of file. The file is ignored in 
repo and should not be committed.

To generate a local `copilot-instructions.md`, run `/create_copilot_instructions`
in GitHub Copilot to get started.

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
