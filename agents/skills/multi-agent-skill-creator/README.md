# Multi-Agent Skill Creator

This directory contains the **Multi-Agent Skill Creator** skill.

It is designed to help engineers create structured, coordinated multi-agent
workflows (skills) for complex, repeatable tasks (coding or non-coding).

It operates under a **Cold Logic** mandate: providing honest, data-driven, and
objective guidance to design optimal workflows, including advising when a
multi-agent approach is unnecessary.

______________________________________________________________________

## How to Use

1. Read [SKILL.md](./SKILL.md) to understand the principles and stages.
2. Invoke the skill by describing your target workflow to Gemini and asking for
   assistance in creating a multi-agent skill for it.
3. Participate in the interactive discovery and design phase.
4. Use the generated templates and validation scripts to bootstrap your new
   skill.

______________________________________________________________________

## Verification & Objectives

To maintain the quality and consistency of this skill creator, it is equipped
with its own validation infrastructure.

### Validation Objectives

Any changes to this directory must satisfy the following criteria:

1. **Markdown Integrity**: All links in [SKILL.md](./SKILL.md) and
   [README.md](./README.md) must be valid relative links. No broken links are
   allowed.
2. **Template Correctness**: Python template files in [templates/](./templates/)
   must be syntactically valid Python code.
3. **Style Consistency**: All Python and Markdown files must be formatted
   according to the local style guides.

### Running Validation Locally

You can run the validation checks locally before committing changes:

```bash
python3 run_presubmit.py
```

This runs the checks defined in [PRESUBMIT.py](./PRESUBMIT.py) using a mock
harness.

______________________________________________________________________

## Files in this Directory

- [SKILL.md](./SKILL.md): The core protocol defining the creation stages and
  rules.
- [README.md](./README.md): This file, documenting usage and verification.
- [PRESUBMIT.py](./PRESUBMIT.py): Static checks for link integrity and template
  syntax.
- [run_presubmit.py](./run_presubmit.py): Local runner for presubmit checks.
- [.style.yapf](./.style.yapf): Python style configuration (PEP8).
- [.style.mdformat](./.style.mdformat): Markdown style marker.
- [templates/](./templates/): Templates used by the skill to generate new
  workflows.
