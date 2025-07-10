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

## Using Prompts

`common.GEMINI.md` can be copied to `chromium/src/GEMINI.md` and modified with
references to prompt templates to add functionality.

For example, if you plan to use ctags to find symbols in the codebase, you can
append `@agents/prompts/templates/ctags.md` to the end of your `GEMINI.md` file
to include ctags instructions. You can confirm the file was imported by using
the `/memory show` command in gemini-cli.
