---
name: analyzing-sql-traces
description: >-
  Extracts raw trace data from Perfetto traces, runs arbitrary SQL queries for
  custom follow-up analysis, and applies expert cognitive principles (Tiered Flow
  Analysis, Semantic Mismatch, Redundancy) to identify performance bottlenecks,
  structural redundancies, and tracer gaps. Use when you need to analyze a trace
  under a specific focus/entrypoint slice, identify uninstrumented 'black boxes',
  or execute arbitrary SQL queries on trace databases directly using SQLite/Perfetto
  SQL syntax, and generate a precise instrumentation breakdown plan or refactoring
  instructions for the Codebase Agent. Don't use for capture or compilation tasks.
---

# Analyzing SQL Traces

A specialized skill for analyzing Perfetto browser traces (individually or
comparatively) to detect performance bottlenecks and generate codebase
refactoring or instrumentation recommendations.

## 1. Prerequisites & Context

- **Treatment Traces** (one or more `.pb` files).
- **Control Traces** (optional, one or more `.pb` files for comparison).
- **Target Slice or Metric Window** (e.g.,
  `Startup.FirstWebContents.FirstContentfulPaint`,
  `OmniboxEditModel::OpenMatch`).
- **Analysis Mode Input**: Select either **`descendants`** (to analyze child
  slices of a specific target) or **`window`** (to analyze all slices
  overlapping a metric window).

### ⚠️ Safety & Sandbox Compliance (Zero-Grant Rule)

To prevent triggering unnecessary user permission/access grant prompts:

- **ALWAYS** write all intermediate and final outputs to the parent E2E
  session's unified analysis directory inside the workspace:
  `out/e2e_nla_run_{parent_session_id}/analysis/` (where `{parent_session_id}`
  is passed by the Orchestrator).
  - Raw data / comparison reports:
    `out/e2e_nla_run_{parent_session_id}/analysis/raw_trace_data.txt` (Mode A,
    Text flamegraph)
    `out/e2e_nla_run_{parent_session_id}/analysis/raw_trace_report.md` (Mode A,
    Markdown report)
    `out/e2e_nla_run_{parent_session_id}/analysis/comparison_report.md` (Mode B,
    Markdown report)
    `out/e2e_nla_run_{parent_session_id}/analysis/comparison_flamegraph.txt`
    (Mode B, Text flamegraph)
  - Structured JSON:
    `out/e2e_nla_run_{parent_session_id}/analysis/trace_analysis_results.json`
  - Markdown Dispatch Report:
    `out/e2e_nla_run_{parent_session_id}/analysis/trace_analysis_dispatch_report.md`
- **NEVER** execute shell utilities like `mkdir`, `ls`, `touch`, or `rm` to
  manage these files.
- **ALWAYS** rely on the internal Python APIs inside `trace_analyzer.py` or
  `trace_comparator.py` to programmatically create directories and manage files
  silently.
- **ALWAYS** run the scripts with
  `vpython3 agents/skills/analyzing-sql-traces/scripts/trace_analyzer.py` or
  `vpython3 agents/skills/analyzing-sql-traces/scripts/trace_comparator.py` to
  avoid extra permmision grant prompts.

______________________________________________________________________

## 2. Core Workflow

### Step 1: Determine the Analysis Mode & Run Extraction

#### Mode A: Single-Group Analysis (Only Treatment Traces Provided)

First, run the trace analyzer to produce an aggregated text flamegraph:

```bash
vpython3 agents/skills/analyzing-sql-traces/scripts/trace_analyzer.py \
  --traces {path/to/treatment_trace_*.pb} \
  --target "{focus_slice_or_metric}" \
  --mode {descendants|window} \
  --format text \
  --output out/e2e_nla_run_{parent_session_id}/analysis/raw_trace_data.txt
```

Second, run the trace analyzer to produce a markdown report with cumulative
redundancy analysis:

```bash
vpython3 agents/skills/analyzing-sql-traces/scripts/trace_analyzer.py \
  --traces {path/to/treatment_trace_*.pb} \
  --target "{focus_slice_or_metric}" \
  --mode {descendants|window} \
  --format markdown \
  --output out/e2e_nla_run_{parent_session_id}/analysis/raw_trace_report.md
```

Read the generated
`out/e2e_nla_run_{parent_session_id}/analysis/raw_trace_data.txt` and
`out/e2e_nla_run_{parent_session_id}/analysis/raw_trace_report.md` using
`view_file`.

#### Mode B: Comparative Analysis (Both Control and Treatment Traces Provided)

First, run the trace comparator to generate the tabular comparative report:

```bash
vpython3 agents/skills/analyzing-sql-traces/scripts/trace_comparator.py \
  --control {path/to/control_trace_*.pb} \
  --experiment {path/to/treatment_trace_*.pb} \
  --target "{focus_slice_or_metric}" \
  --mode {descendants|window} \
  --format markdown \
  --output out/e2e_nla_run_{parent_session_id}/analysis/comparison_report.md
```

Second, run the trace comparator to generate the high-level comparative text
flamegraph (use `--min-dur` to filter out minor slices, e.g., $\\ge 5.0\\text{
ms}$):

```bash
vpython3 agents/skills/analyzing-sql-traces/scripts/trace_comparator.py \
  --control {path/to/control_trace_*.pb} \
  --experiment {path/to/treatment_trace_*.pb} \
  --target "{focus_slice_or_metric}" \
  --mode {descendants|window} \
  --format text \
  --min-dur 5.0 \
  --output out/e2e_nla_run_{parent_session_id}/analysis/comparison_flamegraph.txt
```

Read the generated
`out/e2e_nla_run_{parent_session_id}/analysis/comparison_report.md` and
`out/e2e_nla_run_{parent_session_id}/analysis/comparison_flamegraph.txt` using
`view_file`.

#### Advanced Filtering & Aggregation Options (Optional)

Both scripts (`trace_analyzer.py` and `trace_comparator.py`) support optional
flags to refine slice selection when multiple events share the same name:

- **Aggregation Mode (`--aggregate`)**: If the target slice can be called
  multiple times, use this flag to aggregate all occurrences (cumulative
  durations and self-times) into a single merged call tree.
- **Slice Argument Filtering (`--arg-key <key>` and `--arg-value <value>`)**: To
  analyze only a specific call out of multiple occurrences, filter by its
  arguments (e.g.
  `--arg-key "task.posted_from.file_name" --arg-value "content/browser/browser_main_loop.cc"`).
  *Note:* The `--arg-value` parameter supports SQL `LIKE` operator syntax (e.g.
  `%google.com/search%` to perform prefix or wildcard substring matches).
- **Parent Bounding Target (`--boundary-target <name>`)**: Restricts the target
  slice search to only those occurrences that fall chronologically within the
  execution time windows of a specified parent/boundary event (descendants mode
  only). Use with `--boundary-arg-key <key>` and `--boundary-arg-value <value>`
  to target specific parent navigation/workflow windows.

______________________________________________________________________

### Step 2: Apply Cognitive Principles

Open and **read the mandatory reasoning guide** to evaluate the results,
focusing on browser logic and filtering out infrastructure noise:
`file:///.agents/skills/analyzing-sql-traces/references/cognitive_principles.md`

______________________________________________________________________

### Step 3: Run Arbitrary SQL Queries (Follow-up Analysis)

If you need custom details or want to perform follow-up analysis not covered by
the default trace analyzer/comparator (e.g. searching for specific args, getting
stats on specific threads, custom joins), **ALWAYS** run the arbitrary query
script `query_trace.py` rather than creating a custom script yourself.

#### Usage Guideline

Run the `query_trace.py` helper script using `vpython3`:

```bash
vpython3 agents/skills/analyzing-sql-traces/scripts/query_trace.py \
  --trace {path/to/trace.pb} \
  --query "{sql_query}"
```

Example:

```bash
vpython3 agents/skills/analyzing-sql-traces/scripts/query_trace.py \
  --trace out/Default/trace.pb \
  --query "SELECT name, sum(dur)/1e6 AS total_dur_ms FROM slice GROUP BY name ORDER BY total_dur_ms DESC LIMIT 10;"
```

#### Common Perfetto Tables & Schemas

Here are common SQLite tables available in Perfetto trace databases:

##### `slice` Table

Contains individual track event slices (slices represent synchronous work on a
thread).

- `id` (INT): Unique ID for the slice
- `name` (STRING): Name of the slice / event
- `ts` (INT): Start timestamp in nanoseconds
- `dur` (INT): Duration in nanoseconds
- `track_id` (INT): Track ID on which the slice executed
- `parent_id` (INT): Parent slice ID (if nested)
- `arg_set_id` (INT): ID referencing key-value arguments associated with this
  slice

##### `process` Table

- `upid` (INT): Unique process ID
- `name` (STRING): Name of the process (e.g. Browser, Renderer, GPU Process)
- `pid` (INT): OS process ID

##### `thread` Table

- `utid` (INT): Unique thread ID
- `name` (STRING): Name of the thread (e.g. CrBrowserMain, Compositor)
- `upid` (INT): Parent process ID
- `tid` (INT): OS thread ID

##### `thread_track` Table

- `id` (INT): Track ID
- `utid` (INT): Thread ID associated with this track

##### `args` Table

Contains key-value arguments associated with slices.

- `arg_set_id` (INT): Reference ID matching slice's `arg_set_id`
- `key` (STRING): Hierarchical argument key (e.g. `task.posted_from.file_name`)
- `string_value` / `int_value` / `real_value` (STRING / INT / REAL): Argument
  value

#### Reference Queries

##### Get Top 10 Longest Slices

```sql
SELECT s.name, s.dur / 1e6 AS dur_ms, t.name AS thread_name, p.name AS process_name
FROM slice s
JOIN thread_track tt ON s.track_id = tt.id
JOIN thread t USING(utid)
JOIN process p USING(upid)
ORDER BY s.dur DESC
LIMIT 10;
```

##### List All Processes and Threads in a Trace

```sql
SELECT p.name AS process_name, p.upid, t.name AS thread_name, t.utid
FROM process p
JOIN thread t USING(upid)
ORDER BY process_name, thread_name;
```

##### Find Slices by Name containing a substring

```sql
SELECT name, dur/1e6 AS dur_ms, ts
FROM slice
WHERE name LIKE '%FirstContentfulPaint%'
ORDER BY ts ASC;
```

______________________________________________________________________

## 3. Output Artifact Contracts

You must generate two separate outputs to complete this task:

### Output A: Structured Dispatch JSON

This payload is designed for direct parsing by the Orchestrator to feed to the
Codebase & Instrumentation Agent. Save it to
`out/e2e_nla_run_{parent_session_id}/analysis/trace_analysis_results.json`.

```json
{
  "status": "SUCCESS",
  "analysis": {
    "target_slice": "FocusSliceName",
    "total_duration_ms": 260.6,
    "bottlenecks": [
      {
        "method_name": "CulpritMethodName",
        "severity_score": 8.5,
        "vectors": {
          "critical_path": true,
          "relative_overhead": 0.22,
          "semantic_simplicity": "HIGH" | "MEDIUM" | "LOW",
          "cumulative_redundancy": true
        },
        "breakdown_strategy": {
          "type": "GAP_INSTRUMENTATION" | "FULL_INSTRUMENTATION" | "FLOW_REFACTORING" | "REDUNDANCY_OPTIMIZATION",
          "target_method": "CulpritMethodName",
          "category": "omnibox" | "navigation" | "blink",
          "known_children": ["ChildA", "ChildB"],
          "gap_ms": 12.28,
          "instructions": "Detailed, step-by-step C++ refactoring or instrumentation instructions for the Codebase Agent."
        }
      }
    ]
  }
}
```

### Output B: Markdown Dispatch Report (For Orchestrator Review)

Save a beautifully formatted report to
`out/e2e_nla_run_{parent_session_id}/analysis/trace_analysis_dispatch_report.md`.

- **Format**: Use GitHub-style alerts (`> [!IMPORTANT]`) for the **Codebase
  Agent Dispatch Instructions** to make them stand out.
- **Structure**:
  1. **Executive Summary**: Overall metrics (total time, depth, count of
     bottlenecks).
  2. **Flow-Aware Inefficiencies**: Detailed analysis of slow flows (include
     simple Mermaid diagrams of the redundancy path if applicable).
  3. **Prioritized Bottlenecks**: Ranked list with direct codebase instructions.
  4. **Redundancy Summary Table**: Top 10 repeated operations.
