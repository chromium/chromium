# Prompts

This directory contains a common prompt for Chromium, template prompts to teach
agents about specific tools, and task prompts that were successfully used to
complete a task. Everything is intended to work with gemini-cli.

## Directory Structure

- `common.md`: Common prompt for gemini-cli
- `common.minimal.md`: Core parts that are sub-included by `common.md`
- `templates/`: Reusable snippets of prompts or that can be included in other
  prompts.
- `tasks/`: This directory is intended to hold prompts and plans for complex,
  multi-step tasks. Each subdirectory within `tasks/` represents a specific
  task.

## Creating the System Instruction Prompt

Googler-only docs: http://go/chrome-coding-with-ai-agents

Create a local, untracked file `//GEMINI.md`. Include the relevant
prompts using @, for example, a typical desktop developer will use:

```src/GEMINI.md
@agents/prompts/common.md
@agents/prompts/templates/desktop.md
```

An Android developer would use:

```src/GEMINI.md
@agents/prompts/common.md
@agents/prompts/templates/android.md
```

An iOS developer would use:

```src/GEMINI.md
@agents/prompts/common.md
@agents/prompts/templates/ios.md
```

You can confirm that prompts were successfully imported by running the `/memory
show` command in gemini-cli.

## Known problems

All imports must be scoped to the current prompt file. a/prompt.md can import
a/prompt2.md or a/b/prompt3.md, but cannot import c/prompt4.md. See
https://github.com/google-gemini/gemini-cli/issues/4098.

## Contributing

Please freely add self-contained task prompts and prompt templates that match
the format of the existing examples.

Changes to `common.minimal.md` should be done carefully as it's meant to be used
broadly.

### Custom Commands

Add these to [`//.gemini/commands`].

[`//.gemini/commands`]: /.gemini/commands/README.md
