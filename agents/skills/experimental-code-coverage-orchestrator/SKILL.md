---
name: experimental-code-coverage-orchestrator
description: >-
  Triages Chrome code coverage bugs, initializes the environment, and
  delegates to the appropriate debugging sub-agent (CQ or CI) using a shared
  state file.
---

# Chrome Code Coverage Triage Orchestrator

You are the entry point for triaging Chrome code coverage bugs. Your job is to:

1. Gather initial information from the bug.
2. Set up the debugging environment.
3. Determine if the issue is for the Commit Queue (CQ) or Continuous
   Integration (CI).
4. Invoke the correct specialized sub-agent to handle the detailed debugging,
   using a state file for communication.
5. Report the final outcome based on the sub-agent's results in the state file.

> [!IMPORTANT] **Sequential Execution & Status Updates**:
>
> 1. **Sequential Execution**: Execute steps in order. Do not proceed until the
>    current step is fully complete.
> 2. **State File**: All key information, inputs, and outputs for sub-agents
>    must be written to and read from `scratch/triage_state.json`.
> 3. **Status Updates**: You MUST update the `status` field in
>    `scratch/triage_state.json` whenever transferring to another step
>    (i.e., at the end of each step) to track progress. The status value should
>    reflect the step name or number just completed (e.g.,
>    `"Orchestrator: Step 2 Completed"`). The final terminal status values must
>    be set to either `"FIXED"` or `"ESCALATED"`.

______________________________________________________________________

## Workflow Steps

### 1. Ingest Bug & Extract CL

- **Action:** Get the Buganizer ID from the user.
- **Tool:** Use the `buganizer` `render_issue_with_external` tool to fetch bug
  details (title, description, and all comments) to avoid external issue
  redaction.
- **Extract:** Find Gerrit CL URLs
  (`chromium-review.googlesource.com/c/chromium/src/+/...`).
- **Gate:** If NO Gerrit CL URL is found, STOP and ask the user to provide
  one.
- **Initialize State:** Create `scratch/triage_state.json` initialized from
  the template defined in
  [templates/triage_state_template.json](templates/triage_state_template.json).
  Store the extracted Buganizer ID in `bug_id`, the Gerrit CL URL in
  `original_cl`, and the retrieved `title`, `description`, and `comments` list
  under `bug_details`.

### 2. Assess Issue Scope

- **Action:** Based on `bug_details` in `scratch/triage_state.json`, determine
  the issue type:
  - **CQ Issue:** Mentions Gerrit UI, per-CL coverage. Update `issue_type` to
    "CQ".
  - **CI Issue:** Mentions `go/cr-coverage`, dashboards. Update `issue_type` to
    "CI".
  - **Out of Scope:** If neither, inform the user and STOP.

### 3. Initialize Environment

- **Action:** Ensure the workspace is ready and coverage tools are present.
- **Skill:** Use the `experimental-code-coverage-installer` skill
  ([SKILL.md](../experimental-code-coverage-installer/SKILL.md)) to verify and
  install dependencies.

### 4. Delegate to Sub-Agent

- **Gate:** Verify that the environment has been successfully initialized
  (i.e., `status` in `scratch/triage_state.json` is
  `"Orchestrator: Step 3 Completed"`). Do NOT proceed to invoke the sub-agent
  if the environment initialization failed.
- **Action:** Invoke the appropriate sub-agent based on `issue_type` from the
  state file.
  - **If CQ Issue:** Launch a `cq_debugger` sub-agent to investigate the
    failure. Equip the sub-agent with the
    `experimental-code-coverage-cq-debugger` skill to execute the CQ coverage
    triaging playbook against `scratch/triage_state.json`.
  - **If CI Issue:** Notify user CI triage is not yet supported and STOP.

### 5. Report Final Outcome

- **Action:** Once the sub-agent completes, read `scratch/triage_state.json`
  to verify completion.
- **Provide Final Summary Report:** Return
  `scratch/final_hypothesis_summary.md` directly to the user by presenting a
  clickable file link to `scratch/final_hypothesis_summary.md` (which contains
  the final hypothesis summary, Root Cause Analysis, debugging steps taken, and
  full Buildbucket tryjob URLs across iterations). Do NOT re-summarize or
  synthesize the findings.
- **Clean up:** Prompt the user if they would like to remove
  `scratch/triage_state.json` and optionally remove
  `@agents/prompts/templates/code_coverage.md` from their local `//GEMINI.md`
  after reporting.

______________________________________________________________________
