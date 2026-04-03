---
name: cl-description
description: Use this skill to draft, write, or format a Changelist (CL) description or commit message strictly following Chromium's guidelines. Trigger this whenever the user needs help documenting their code changes for review, even if they don't explicitly mention "Chromium" or "CL". Do not use this skill for formatting code, explaining logic, or performing general code reviews.
---

Follow the instructions in the file
`agents/skills/cl-description/cl-description.md` (read it using its exact
relative path from the repository root) carefully to generate the CL
description. Ensure that all the constraints specified in the template are met.

### Programmatic Line Wrapping Rule (Mathematical Wrapping)

Because LLMs cannot reliably hard-wrap text at precisely 72 characters, you
**MUST** mathematically format your draft before presenting the final response.
Use the provided Python script
`agents/skills/cl-description/scripts/wrap_lines.py` via your execution tools.
Pre-requisite: Save your draft to a file (e.g. `draft.txt`). Example usage:
`vpython3 agents/skills/cl-description/scripts/wrap_lines.py draft.txt` Final
output should be the code block containing the mathematically formatted text.
