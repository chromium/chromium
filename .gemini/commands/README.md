# Gemini Custom Commands for Chrome

See: https://cloud.google.com/blog/topics/developers-practitioners/gemini-cli-custom-slash-commands

## Naming

The top-level directory is called "cr" (short for Chrome) so that "/cr:" will
show all available custom commands. It is not necessary for custom commands to
be put into subdirectories, but use them if it helps.

## What Belongs Here?

Any prompt that is not a one-off could be put here. One-off prompts that should
be used as examples should go in `//agents/prompts/eval`.
