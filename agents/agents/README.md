# Chromium Agents

This directory contains reusable agent and subagent definitions for Chromium
development.

## Agents

- [`code_reviewer.md`](code_reviewer.md): Subagent that reviews local diffs,
  commits, branches, or Gerrit CLs using
  [`code-review`](/agents/skills/code-review/SKILL.md) (with
  [`references/gerrit_review.md`](/agents/skills/code-review/references/gerrit_review.md)
  for Gerrit CLs), delegating in-tree rules review to
  [`ruled_code_reviewer.md`](ruled_code_reviewer.md) and security-sensitive
  changes to [`security_reviewer.md`](security_reviewer.md).
- [`ruled_code_reviewer.md`](ruled_code_reviewer.md): Subagent that audits
  changes against directory-specific in-tree Knowledge Base rules and review
  skills using [`ruled-code-review`](/agents/skills/ruled-code-review/SKILL.md).
- [`security_reviewer.md`](security_reviewer.md): Subagent that audits changes
  for security vulnerabilities and sibling variations using
  [`security-review`](/agents/skills/security-review/SKILL.md).

## Register Agents

Add this directory to your `.agents/agents.json` or team harness manifest:

```json
{
  "entries": [
    {
      "path": "agents/agents"
    }
  ]
}
```

Run from `src/` to add `agents/agents` to `.agents/agents.json`:

```bash
mkdir -p .agents
[ -s .agents/agents.json ] || echo '{}' > .agents/agents.json
jq 'if (.entries // []) | any(.path? == "agents/agents") then .
    else .entries += [{"path": "agents/agents"}] end' \
  .agents/agents.json > .agents/agents.json.tmp &&
  mv .agents/agents.json.tmp .agents/agents.json
```
