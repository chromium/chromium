# MAGI Module Routing Index

This file acts as a routing catalog for the Orchestrator. It identifies the
technical rulesets and implementation modules available for execution and
auditing.

## Execution Agents

These modules are responsible for investigation, scaffolding, and code
synthesis.

- **Scoping:** Investigation, codebase research, goal definition. *Path:*
  `personas/core/scoping.json`
- **Synthesis:** Maintainability, Chromium idioms, `//base` primitives, and
  final code synthesis. *Path:* `personas/core/implementation.json`

## Scanners (Auditors)

Specialized experts who perform rigorous, boolean-checklist-based audits.

- **The Security Scanner:** Memory safety, exploit prevention, logic. *Path:*
  `personas/core/security.json`
- **The Performance Scanner:** Latency, zero-copy, sequence affinity. *Path:*
  `personas/core/performance.json`
- **The Core Auditor:** Consistency with existing patterns and idioms. *Path:*
  `personas/core/auditor.json`
- **The Refactoring Auditor:** Correctness and completeness during refactoring
  (tests, assertions, comments). *Path:*
  `personas/core/refactoring_auditor.json`
- **The Test Expert:** Testability, edge-cases, framework usage. *Path:*
  `personas/auxiliary/test.json`
- **The Concurrency Expert:** `base::PostTask` safety, preventing deadlocks.
  *Path:* `personas/auxiliary/concurrency.json`
- **The Privacy Expert:** PII prevention, UMA/UKM metrics. *Path:*
  `personas/auxiliary/privacy.json`
- **The Build Expert:** `DEPS` compliance, `#include` bloat, GN boundaries.
  *Path:* `personas/auxiliary/build.json`
- **The Readability Expert:** Clean code, naming, cognitive complexity. *Path:*
  `personas/auxiliary/readability.json`

## Platform Specialists

Use these when modifying platform-specific implementations.

- **Windows File API Expert:** `ScopedHandle`, file locking, security
  descriptors. *Path:* `personas/windows/file_api.json`
- **Linux Wayland Expert:** `wl_buffer` management, protocol integration.
  *Path:* `personas/linux/wayland.json`

## Domain Specialists

Use these when modifying specific technical domains like media or networking.

- **WebRTC Expert:** `PeerConnection`, signaling, WebRTC threading model.
  *Path:* `personas/domain/webrtc.json`
- **Codec (AV1) Expert:** `libaom` settings, bitrate adaptation, latency.
  *Path:* `personas/domain/codec_av1.json`

## System Meta-Scanners

Use these when auditing or modifying the MAGI protocol itself.

- **LLM Behavior & Grounding Expert:** Hallucination prevention, prompt
  engineering, state machine safety. *Path:* `personas/ai/llm.json`
- **MAS Architect:** Multi-agent system architecture, handoff stability,
  verification loop efficiency. *Path:* `personas/ai/mas.json`
