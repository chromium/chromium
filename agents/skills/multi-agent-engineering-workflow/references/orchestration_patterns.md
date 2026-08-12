# Harness-Specific Drivers (Orchestration Patterns)

The Orchestrator MUST adjust its behavior and instruction set based on the
`project.magi.json#environment/orchestration_pattern` to optimize for the
specific CLI harness.

## 1. CENTRALIZED (Simulated MAS / JETSKI)

Optimized for single-threaded harnesses without native background routing.

- **The State Driver:** The Orchestrator MUST act as the primary state machine.
  It MUST manually parse `next_stage` signals from sub-agent JSON outputs and
  explicitly invoke the subsequent tools.
- **Aggressive Batching (The 1-Turn Rule):** To minimize human-in-the-loop wait
  times, the Orchestrator MUST attempt to pack all independent operations for a
  stage into a single turn. This includes reading all necessary personas, source
  files, and state files in parallel (`wait_for_previous: false`).
- **Proactive State Recovery:** The Orchestrator MUST start every turn by
  reading `state_block.magi.json` to ground its context, making the protocol
  resilient to turn interruptions or context loss.
- **Direct Prompt Injection:** The Orchestrator SHOULD read the
  `personas/**/*.json` files and inject their `mandate` and `checklist` directly
  into the sub-agent invocation prompts to save turns. *Joining Rule:* If a
  mandate or checklist item is an array of strings, the Orchestrator MUST join
  them using direct concatenation (`"".join(array)`). To prevent token merging,
  each element in the array (except the last) MUST end with a trailing space or
  punctuation.

## 2. DECENTRALIZED (True MAS / GENERIC_CLI)

Optimized for autonomous harnesses with native multi-agent routing (e.g., Gemini
CLI).

- **Autonomous Delegation:** The Orchestrator MUST delegate task execution to
  the harness's native sub-agent tools. It SHOULD NOT manually drive every minor
  transition.
- **Signaling over Coordination:** Agents SHOULD use `next_stage` signals to
  trigger successor agents directly through the harness.
- **Lean Monitoring:** The Orchestrator's context SHOULD remain lean. It
  monitors high-level "Checkpoints" (e.g., `project.magi.json` and
  `state_block.magi.json` updates) rather than every interim tool call.
- **Parallel Synthesis:** Leverage the harness's ability to run multiple
  specialized agents in parallel without centralized serialization.
