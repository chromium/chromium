# Routing Configuration

Maps feedback context and research tasks to the appropriate training personas.

## Core Training Roles

- **Analyzer:** `personas/core/analyzer.json` (Used for Gap Analysis in all
  paths: Basic, Deep, and Breadth).
- **Upgrader:** `personas/core/upgrader.json` (Used for writing changes and
  validating schemas).

## Parallel Research Roles (Breadth Path)

- **Architect:** `personas/core/architect.json` (Used for Static Architecture
  analysis of docs/builds).
- **History Miner:** `personas/core/history_miner.json` (Used for sampling CL
  history and comments).
- **Usage Analyzer:** `personas/core/usage_analyzer.json` (Used for scanning
  external API consumers).
- **Consolidator:** `personas/core/consolidator.json` (Used for collating and
  deduplicating parallel findings).
