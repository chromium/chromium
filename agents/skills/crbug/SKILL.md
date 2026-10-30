---
name: crbug
description: >-
  Triggers when the user mentions, asks to open/fetch, or requests action on
  (e.g., fix, investigate, resolve) a Chromium issue or bug report (e.g.,
  crbug.com/123456, https://issues.chromium.org/issues/123456, bug 123456, or
  issue 123456). Fetches public issue metadata, description, and comments from
  issues.chromium.org (including resolving legacy Monorail IDs via crbug.com)
  and gracefully handles restricted or unreachable issues.
---

# Chromium Issue (crbug) Fetcher

When the user mentions, asks to open/fetch, or requests action on (e.g., fix,
investigate, resolve) a Chromium bug or issue, follow this workflow to retrieve
the issue context.

> **Internal / Restricted Issues (HTTP 401/403):** If the user references a
> Google-internal issue tracker URL or if `fetch_crbug.py` reports that the
> issue requires authentication (restricted visibility), use the internal
> `buganizer-ticket` skill if available in your workspace.

______________________________________________________________________

## Usage

Match any occurrences of:

- `(?:https?://)?crbug\.com/(?:[a-zA-Z0-9_-]+/)?\d+` (e.g. `crbug.com/40256175`,
  `https://crbug.com/1422675`, `crbug.com/v8/12345`)
- `https?://issues\.chromium\.org/(?:u/\d+/)?issues/\d+` (e.g.
  `https://issues.chromium.org/issues/40256175`)
- `https?://bugs\.chromium\.org/p/[a-zA-Z0-9_-]+/issues/detail\?id=\d+`
- `\bissues?[/\s]+\d+\b` (e.g. `issue 40256175`, `issues/40256175`)
- `\bbug\s+\d+\b` (e.g. `bug 40256175`)

______________________________________________________________________

## Step-by-Step Workflow

### Step 1: Extract Issue ID

Extract the numeric issue ID or URL (e.g. `40256175` or `crbug.com/1422675`)
from the user prompt. Both migrated IssueTracker IDs (`>= 10000000`) and legacy
Monorail IDs (`< 10000000`) are supported.

______________________________________________________________________

### Step 2: Run the Python Extraction Script

Execute `fetch_crbug.py` from the Chromium `src` directory:

- **Linux / macOS**:
  ```bash
  vpython3 agents/skills/crbug/scripts/fetch_crbug.py <ISSUE_ID_OR_URL>
  ```
- **Windows**:
  ```cmd
  vpython3 agents\skills\crbug\scripts\fetch_crbug.py <ISSUE_ID_OR_URL>
  ```

*(If skills are symlinked into `.agents/skills/`, you may also invoke
`.agents/skills/crbug/scripts/fetch_crbug.py`).*

By default, the bodies of `<details>` blocks (e.g. full commit details posted by
the Git Watcher bot) are elided, keeping only their `<summary>`. If an elided
block is relevant, rerun the script with `--expand-details`.

> **Note (Sandbox & Network Requirements):** `fetch_crbug.py` makes outbound
> HTTPS requests to `crbug.com` and `issues.chromium.org`. If your agent
> environment blocks network access inside a sandbox, run the script outside the
> sandbox.

______________________________________________________________________

### Step 3: Format and Present Issue Details

Format the extracted issue data into clean, structured Markdown:

1. **Header & Metadata**:
   - Issue ID with clickable link (`https://crbug.com/<ID>`)
   - Title
   - Component ID
   - Status
   - Type, Priority, Severity
   - Reporter and Assignee
2. **Description**: The original issue description and reproduction steps.
3. **Recent Comments**: Key updates or discussions from the comment thread.

______________________________________________________________________

## Error & Failure Handling

### Restricted Issues (HTTP 401 / 403)

If `fetch_crbug.py` fails because the issue has restricted access (e.g.,
histogram-expiry bugs, security bugs, or internal tickets):

1. Check if the internal `buganizer-ticket` skill is available in your
   environment and use it to fetch the issue with authenticated credentials.
2. Otherwise, follow the **Retrieval Failure Guardrails** below.

### Retrieval Failure Guardrails

If `fetch_crbug.py` (and `buganizer-ticket`, if available) fails to retrieve the
issue:

1. **Do not hallucinate or guess** the contents of the issue.
2. **Do not run raw shell commands** (`curl`, `wget`, etc.) against `crbug.com`,
   `issues.chromium.org`, or `issuetracker.google.com`.
3. **Prompt the user directly** to provide the issue details manually:
   > "Unable to retrieve Chromium issue `<ISSUE_ID>` automatically. Please
   > provide the issue details directly (such as title, description,
   > reproduction steps, or expected vs. actual behavior) so we can proceed."
