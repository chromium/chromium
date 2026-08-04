# Review Consolidation & Oscillation Detection

This document defines the exact logic for merging scanner checklists and
detecting conflicts.

## Logical AND Consolidation

When consolidating multiple scanner checklists, the Consolidation agent must
apply a strict Logical AND.

1. **Keys of Interest:** Collect the set of all checklist keys evaluated by the
   active scanners.
2. **Evaluation:**
   - If a key was evaluated by only one scanner, its status in the consolidated
     checklist is the value asserted by that scanner.
   - If a key was evaluated by multiple scanners, its status in the consolidated
     checklist is `true` if and only if **all** scanners asserted `true`. If any
     scanner asserts `false`, the consolidated key is `false`.
3. **Actionable Constraints:** For every `false` key, generate a concrete
   instruction in `constraints.magi.json` describing what must be fixed.

## Oscillation Detection

Oscillation occurs when the refinement loop is stuck in an infinite cycle due to
conflicting scanner mandates (e.g., the Security Scanner demands an extra
validation check that the Performance Scanner rejects because of latency).

The Consolidation agent must set `oscillation_detected = true` if:

1. **Checklist Key Toggle:** A specific checklist key toggles state (`true` ->
   `false` -> `true`) across consecutive iterations.
2. **Static Constraints:** The generated `constraints.magi.json` file is
   identical in content across two consecutive iterations, indicating no
   progress is being made.

### Escalation Protocol

When `oscillation_detected` is set to `true`:

1. Stop the execution loop.
2. Write a `conflict_report` containing:
   - The checklist key in conflict.
   - The scanners involved.
   - The conflicting reasons provided by the scanners.
3. Exit with `next_stage: ESCALATION`.
