# Stage 1: Specify & Investigate

## Step 1: Define Goal (Scoping)

1. **The Investigation:** When a bug or feature is requested, the Orchestrator
   MUST NOT read the raw logs or attempt to hold the requirements in its own
   context window. Instead, invoke a "Scoping" sub-agent.
2. **Session Resumption:** Before starting investigation, Scoping MUST check if
   the directory configured in `temp_directory` contains a half-finished session
   (e.g., existing state files). If it does, the agent MUST ask the user whether
   they want to resume the previous session or start a new task.

## Step 2: Investigate Codebase (Scoping)

1. Scoping investigates the codebase (`grep_search`, `read_file`) to understand
   context, dependencies, and existing patterns.
2. **Environment Discovery:** Before writing the file, Scoping MUST discover the
   environment:
   - **VCS:** Check for a `.jj/` directory or run `jj status`. If successful,
     set `vcs` to `"JJ"`. Otherwise, default to `"GIT"`.
   - **Harness:** Check if Jetski tools (e.g., `code_search`, `view_file`) are
     available. If yes, set `harness` to `"JETSKI"`. Otherwise, set to
     `"GENERIC_CLI"`.
   - **Temp Directory:** Configure the path where all transient MAGI files will
     be stored:
     - If Harness is `"JETSKI"`, check the environment variable
       `$ANTIGRAVITY_CONVERSATION_ID` and set `temp_directory` to the JetSki
       conversation brain folder:
       `~/.gemini/jetski/brain/<conversation_id>/.temp/` (must be resolved to
       its absolute path).
     - If Harness is `"GENERIC_CLI"`, default to
       `agents/skills/magi-mode/.temp/`.

## Step 3: Define Scope (Scoping)

1. Scoping writes a strict specification to `project.magi.json` conforming to
   `magi_schema.json`.
2. **Path & Complexity Determination:** Scoping MUST determine the
   `execution_path` and `complexity_level`:
   - `complexity_level`: `LOW` (minor bug fixes, small nits), `MEDIUM` (standard
     feature work), or `HIGH` (architectural changes, security-sensitive code).
   - `execution_path`: `FAST_PATH` if complexity is `LOW` and ambiguity is
     `LOW`. Otherwise, default to `RIGOR_PATH`.
3. **Task Type Determination:** Scoping MUST determine the `task_type` based on
   the request:
   - `IMPLEMENTATION`: Default. For creating new features or fixing bugs. Sets
     `next_stage` to `SCAFFOLDING`.
   - `REVIEW`: For reviewing existing changes or a CL. Sets `next_stage` to
     `PREPARATION`.
   - `AUDIT`: For analyzing existing code for modernization or flaws. Sets
     `next_stage` to `PREPARATION`.
   - `REFACTORING`: For restructuring existing code without changing behavior
     (e.g., splitting files, extracting classes). Sets `next_stage` to
     `PREPARATION`.
4. **Context Resolution (The Stop-and-Verify Gate):** Scoping MUST verify that
   all external context (e.g., Buganizer links, documentation URLs) has been
   successfully retrieved and parsed. If any link returned a login prompt,
   redirect, or error, Scoping MUST halt, report the failure to the human, and
   request the raw text of the missing context. It MUST NOT proceed with a
   hallucinated or "fallback" scope.
5. **Approach Confirmation (The Dynamic Gate):** Scoping MUST assess the
   ambiguity of the request.
   - **Low Ambiguity:** If the human provided prescriptive instructions (e.g.,
     "Add feature X to file Y"), Scoping sets `ambiguity_level: "LOW"`.
   - **High Ambiguity:** If the request is exploratory, implies multiple viable
     paths, or is underspecified, Scoping sets `ambiguity_level: "HIGH"`. **The
     Gate:** If `ambiguity_level == "HIGH"` OR `context_resolved == false`, the
     Orchestrator MUST pause for human intervention. If
     `ambiguity_level == "LOW"` AND `context_resolved == true`, the Orchestrator
     MAY auto-proceed to the next stage.
   - **Confirmation Details:** During the gate, Scoping MUST present the
     discovered `goal`, `target_files`, and **Build Parameters** (Debug/Release,
     reclient status, and `output_directory`) to the user for verification.
6. **Refactoring Metadata:** If `task_type` is `REFACTORING`, Scoping MUST
   populate `refactoring_meta` in `project.magi.json` with the `original_commit`
   and `file_mappings` to trace code movement.
7. **Routing Overlay:** If the project requires internal scanners (e.g., in
   Google3), Scoping SHOULD populate `routing_overlay` in `project.magi.json`
   with the G3 routing path (e.g.,
   `google3/chrome/remoting/skills/magi_mode/ROUTING_CHROME.md`).
8. **JSON Contract (`project.magi.json`):** See [EXAMPLES.md](../EXAMPLES.md)
   for a full example. *Tooling Selection:* The combination of `repo_type`,
   `vcs`, and `harness` in the `environment` block determines the exact build,
   test, and upload commands used by the agents.
