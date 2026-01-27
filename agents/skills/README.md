# Agent Skills

This directory contains specialized Agent Skills for Chromium development.
Unlike general context files, skills are "on-demand" expertise that the Gemini
CLI can activate when relevant to your request.

## How to Use

To use a skill, you must first install it into your workspace. Creating a
symlink is preferred so that the skill stays up-to-date when you sync your
local checkout:

```bash
mkdir -p .gemini/skills
ln -s $(pwd)/agents/skills/<skill-name> .gemini/skills/
```

Once installed, Gemini will automatically detect when a skill is relevant to
your request and ask for permission to activate it.

## Contributing

New skills should be self-contained within their own directory under
`agents/skills/`. Each skill requires a `SKILL.md` file at its root with a name
and description in the YAML frontmatter.
