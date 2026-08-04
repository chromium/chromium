# Synthesis, Merging and Compile Gates

This document defines how the Synthesis agent resolves conflicts and ensures
build integrity.

## 3-Way Merge Strategy

When merging parallel implementation drafts, the Synthesis agent MUST NOT
perform full-file overwrites. It must use a 3-way merge approach.

1. **Identify the Ancestor:** The "Base Scaffold" (Stage 2 Snapshot) is the
   common ancestor.
2. **Draft Comparison:** Compare the modified files from parallel agents (e.g.
   Security Writer vs Performance Writer) against the Base Scaffold.
3. **Merge Logic:**
   - If a line is modified by only one agent, accept that modification.
   - If a line is modified by both agents in an identical way, accept it.
   - If there is a conflict (different modifications to the same block),
     Synthesis must analyze the semantic meaning and resolve it without removing
     necessary assertions or checks.
   - **Signature Lock:** Synthesis MUST NOT change function signatures
     established in the Scaffold. If a signature change is unavoidable, report
     `ESCALATION` to the parent orchestrator.

## The Compile Gate

The Compile Gate is an empirical check.

1. **Execution:** Synthesis runs the build command for the configured
   `build_targets` in `project.magi.json`.
2. **Analysis:**
   - **Green Build:** If the compilation succeeds, Synthesis records the build
     logs and proceeds to test execution.
   - **Red Build:** If compilation fails (syntax errors, missing headers, linker
     errors):
     - Synthesis parses the compiler error output.
     - It writes the error trace to a local diagnostic file.
     - It loops back to the implementation agents, providing the diagnostic file
       as a constraint.
3. **Loop Limit:** This compile-fix loop is limited to 3 attempts.
