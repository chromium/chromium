# Code Review Module Routing Index

This file acts as a routing catalog for the Code Review Orchestrator. It
identifies the technical rulesets available for auditing.

## Scanners (Auditors)

Specialized experts who perform rigorous, boolean-checklist-based audits.

- **The Security Scanner:** Memory safety, exploit prevention, logic. *Path:*
  `personas/core/security.json`
- **The Performance Scanner:** Latency, zero-copy, sequence affinity. *Path:*
  `personas/core/performance.json`
- **The Core Scanner:** Consistency with existing patterns and idioms. *Path:*
  `personas/core/auditor.json`
- **The Refactoring Scanner:** Correctness and completeness during refactoring
  (tests, assertions, comments). *Path:*
  `personas/core/refactoring_auditor.json`
