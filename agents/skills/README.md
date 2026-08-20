# Agent Skills

This directory contains specialized Agent Skills for Chromium development.

Unlike general context files, skills are shared, "on-demand" expertise that
multiple AI agents (such as Antigravity, Claude, etc.) can activate when
relevant to your request.

See https://agentskills.io/ for general information about the agent skills.

## How to Use

### Local agents that read `.agents/skills`

Agents that discover skills in `.agents/skills`, such as Gemini CLI and GitHub
Copilot, can use the setup script to link skills from this directory:

```sh
agents/skills/setup.py link gerrit-cli histograms
agents/skills/setup.py uninstall gerrit-cli
```

The links are created under `.agents/skills/`, which is gitignored, so they only
affect your local checkout.

The `list`, `enable` and `disable` commands are implemented with the Gemini CLI
and require it to be installed.

### Antigravity

To use these skills with Antigravity, register them in your workspace's
`.agents/skills.json` file (created at your project root). To add all skills:

```json
{
  "entries": [
    {"path": "agents/skills"}
  ]
}
```

Or to only add individual skills:

```json
{
  "entries": [
    {"path": "agents/shared/skills/gerrit-cli"},
    {"path": "agents/skills/histograms"}
  ]
}
```

Once registered, Antigravity 2.0 and `agy` will automatically discover and load
the skills inside this folder. The agent will detect when a skill is relevant to
your request and prompt for permission to activate it.

## Contributing

New skills should be self-contained within their own directory under
`agents/skills/`. Each skill requires a `SKILL.md` file at its root with a name
and description in the YAML frontmatter.

Most skills can be written or improved by asking the Antigravity agent to do so.
