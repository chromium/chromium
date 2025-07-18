# Prompts

This directory contains a common prompt for Chromium, template prompts to teach
agents about specific tools, and task prompts that were successfully used to
complete a task. Everything is intended to work with gemini-cli.

## Directory Structure

- `common.GEMINI.md`: Contains global, high-level context and instructions for
  the agent with general guidelines for interacting with the Chromium project.
- `tasks/`: This directory is intended to hold prompts and plans for complex,
  multi-step tasks. Each subdirectory within `tasks/` represents a specific
  task.
- `templates/`: This directory contains reusable snippets of prompts or
  instructions that can be included in other prompts.

## Creating the System Instruction Prompt

Create a local, untracked file `chromium/src/GEMINI.md`. Include the relevant
prompts using @, for example, a typical desktop developer will use:

```src/GEMINI.md`
@agents/prompts/common.GEMINI.md
@agents/prompts/templates/desktop.md
```

You can confirm that prompts were successfully imported by running the `/memory
show` command in gemini-cli.

## Known problems

All imports must be scoped to the current prompt file. a/prompt.md can import
a/prompt2.md or a/b/prompt3.md, but cannot import c/prompt4.md. See
https://github.com/google-gemini/gemini-cli/issues/4098.
