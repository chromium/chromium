---
name: histogram-extension
description: Extension pipeline to bump expiration dates of Chromium UMA histograms.
---
# histogram-extension
## Overview
This skill outlines how you (the agent) should automatically locate and extend the expiration dates of UMA histograms in Chromium that have expired or are nearing expiration.
## Workflow
### 1. Discovery & Locate (Delegated)
- **AI-Led Discovery:** Delegate to the **`generalist`** sub-agent to find histograms that are nearing expiration if none are provided. Run:
  ```bash
  python3 scripts/find_candidates.py --count 1
  ```
- **Bug Checking:** Check `references/bug_discovery.md` and Buganizer to ensure the metric isn't already being extended in another ticket.
- **Locate:** Look inside `tools/metrics/histograms/metadata/*/histograms.xml` to find the target metric.
- Look for the `<histogram name="TARGET_NAME" expires_after="YYYY-MM-DD">` definition (or `enum=... expires_after="..."`).
### 2. Update the Expiration Date
- Ask the user (or read from the brief) what the new expiration date should be. If none is specified, recommend extending by +6 months or +1 year (in `YYYY-MM-DD` format).
- Carefully replace the `expires_after="YYYY-MM-DD"` string constraint inline, being sure not to modify other attributes.
### 3. Commit and CL creation
- Create the Git commit with a descriptive, code-health aligned message using the metadata template below.

- **Skill Name:** `histogram-extension`
- **Branch Name:** `histogram-extension-[histogram-name]`
- **Commit Hashtag:** `Code Health`
- **Cleanup Title:** `Extend expiration for [Histogram Name]`
- **Cleanup Description:**
  `This metric is still actively needed for telemetry monitoring. Extending expiration to prevent data loss.`
- **Parent Bug:** `499059543`
- **Bug ID:** The resolved `<Bug ID>` from the Bug Checking step.

- Upload the CL to Gerrit and add the appropriate metadata reviewers depending on the `OWNERS` file located in `tools/metrics/histograms/metadata/{category}/OWNERS`.
