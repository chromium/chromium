# TDD Implementation Module Routing Index

This file acts as a routing catalog for the TDD Orchestrator. It identifies the
technical rulesets available for execution and testing bounds.

## Execution Agents

These modules are responsible for investigation, scaffolding, and code
synthesis.

- **Scoper:** API design, code scaffolding, and compilation verification.
  *Path:* `personas/core/scoper.json`
- **Synthesis:** Maintainability, Chromium idioms, `//base` primitives, and
  final code synthesis. *Path:* `personas/core/implementation.json`
- **Test Expert:** Testability, edge-cases, framework usage. *Path:*
  `personas/core/gtest.json`
