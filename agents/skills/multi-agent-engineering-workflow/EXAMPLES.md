# Workflow Example Configurations

This file contains full JSON examples for configurations used in the Multi-Agent
Engineering Workflow protocol.

## `project.workflow.json`

```json
{
  "$schema": "./workflow_schema.json#definitions/ProjectSpec",
  "task_type": "IMPLEMENTATION",
  "execution_path": "RIGOR_PATH",
  "complexity_level": "MEDIUM",
  "goal": "A one-sentence summary of the fix/feature.",
  "target_files": [
    "Repository-relative paths to modify (e.g., ['chrome/browser/...'])."
  ],
  "anti_goals": ["What should explicitly NOT be changed."],
  "edge_cases": ["Specific warnings from logs or code context."],
  "build_targets": ["//chrome:chrome"],
  "context_resolved": true,
  "approach_confirmed": true,
  "ambiguity_level": "LOW",
  "environment": {
    "repo_type": "CHROMIUM",
    "vcs": "JJ",
    "harness": "JETSKI",
    "orchestration_pattern": "CENTRALIZED",
    "use_reclient": true,
    "is_debug_build": true,
    "output_directory": "out/Default",
    "temp_directory":
      "/usr/local/google/home/<user>/.gemini/jetski/brain/<id>/.temp"
  }
}
```

## `state_block.workflow.json`

```json
{
  "$schema": "./workflow_schema.json#definitions/StateBlock",
  "checklist": {
    "[Merged keys from selected rulesets]": false
  },
  "iteration": 1,
  "oscillation_detected": false,
  "conflict_report": [],
  "active_constraints": [],
  "resolved_constraints": [],
  "unlisted_issues_found": [],
  "next_stage": "[Determined by task type]",
  "state_transport": "[FILE_IO/EPHEMERAL_WITH_LOGS]"
}
```
