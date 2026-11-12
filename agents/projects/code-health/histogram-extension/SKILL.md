---
name: histogram-extension
description: Extension pipeline to bump expiration dates of Chromium UMA histograms.
---

# histogram-extension

## Overview

This skill outlines how you (the agent) should automatically locate and extend
the expiration dates of UMA histograms in Chromium that have expired or are
nearing expiration.

## Workflow

### 1. Discovery & Locate

- **Discovery:** If no histogram is provided, run directly:
  ```bash
  python3 agents/projects/code-health/histogram-extension/scripts/find_candidates.py --count 1
  ```
- **Bug Checking:** Check
  [references/bug_discovery.md](references/bug_discovery.md) and Buganizer to
  ensure the metric isn't already being extended in another ticket.
- **Locate:** Look inside `tools/metrics/histograms/metadata/*/histograms.xml`
  to find the target metric.
- Look for the `<histogram name="TARGET_NAME" expires_after="YYYY-MM-DD">`
  definition (or `enum=... expires_after="..."`).

### 2. Update the Expiration Date

- Use `ask_question` (unless already specified in the prompt/brief) to ask what
  the new expiration date should be. If none is specified, recommend extending
  by +6 months or +1 year (in `YYYY-MM-DD` format).
- Carefully replace the `expires_after="YYYY-MM-DD"` string constraint inline,
  being sure not to modify other attributes.

### 3. Submission

#### 1. Prepare Commit Message

Generate `../commit`. Use the resolved `<Bug ID>` from the Bug Checking step for
`--bug`, or `"none"` if no bug was found or filed (the script then emits only
the parent bug).

```bash
python3 agents/projects/code-health/hub/scripts/prepare_commit_message.py \
  --skill histogram-extension \
  --title "Extend expiration for <HistogramName>" \
  --bug "<Bug ID>" \
  --description - \
  --out-commit ../commit <<'EOF'
This metric is still actively needed for telemetry monitoring. Extending
expiration to prevent data loss.
EOF
```

#### 2. Validate Commit File

```bash
python3 agents/projects/code-health/hub/scripts/prepare_commit_message.py \
  --skill histogram-extension \
  --validate ../commit \
  --bug "<Bug ID>"
```

If validation fails, fix the commit title or description and re-run Steps 1–2
until validation exits 0.

#### 3. Submit CL

Invoke the [Submission](../hub/references/submission.md) workflow, passing:

- **Branch Name:** `histogram-extension-<HistogramName>`
- **Commit File:** `../commit`
- **Upload Flags:** Reviewer flags (e.g., `--reviewers <owners>`) based on the
  `OWNERS` file located in
  `tools/metrics/histograms/metadata/{category}/OWNERS`.
