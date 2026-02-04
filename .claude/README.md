# Claude Code Configuration

This directory provides configuration for Claude Code in the Chromium codebase.

## Code Layout
- [.claude/skills/](./skills/): Skills for Claude Code. Source files are located
  in `//agents/skills/`. To install a skill locally, symlink or copy its
  `SKILL.md` to this directory.

## Contributing Guidelines
New skills should be added under `//agents/skills/`. See its README.md for details.
