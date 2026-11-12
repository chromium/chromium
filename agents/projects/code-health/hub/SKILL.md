---
name: hub
description: Orchestrator for the "Code Health Hub" framework. Trigger only when the user refers to the Hub or asks to see "available cleanup tasks" within the Chromium technical debt reduction system.
---

# 🛠️ Chrome Code Health Hub

Act as the Orchestrator for Chrome/Clank Code Healths. The goal is to make it
frictionless for engineers to contribute outside their immediate area.

## Workflow

### 1. Welcome & Triage

When activated, immediately greet the user and use the `ask_question` tool to
present a menu of contribution categories.

**Options to present:**

1. **🧹 Routine Cleanup (Code Health)** - Remove expired histograms, fix outdated
   Javadocs, clean up inline FQNs, and more.
2. **🔍 Technical Debt & Polish** - Fix specific bugs, address flaky tests, or
   improve UI components. *(Coming soon)*
3. **🌱 Discovery & Citizenship** - Find open bugs to claim or browse tech
   rotations. *(Coming soon)*

If the user selects a category marked as *(Coming soon)*, clearly inform them
that this workflow is still under development and ask them to choose another
option.

### 2. Category A Routing (Code Health)

If the selection is Category A, use the `ask_question` tool again to prompt for
a specific Code Health task:

1. **Histogram Cleanup** - Remove metadata and recording sites for expired
   metrics.
2. **Histogram Extension** - Bump the expiration dates of UMA histograms.
3. **Accessibility Cleanup** - Fix common Android accessibility API violations
   in Clank Java code.
4. **Fix Outdated Javadocs** - Update mismatched or stale Javadoc comments.
5. **Java Inline FQN Cleanup** - Replace inline fully qualified Java names with
   imports.
6. **Remove Unused Object Overrides** - Remove unreferenced `equals`,
   `hashCode`, and `toString` overrides.
7. **Keep Sorted** - Detect misordered lists and add `go/keep-sorted` tags.
8. **Lint Sync Guards** - Add IfChange/ThenChange guards to keep enums in sync.
9. **Feature Flag Cleanup** - Safely remove code for fully launched or abandoned
   flags. *(Coming soon)*

#### Handoff for Category A:

If the user selects an option marked as *(Coming soon)*, clearly inform them
that the skill is not yet available and ask them to choose another task.

For available tasks, do not instruct the user to activate the skill manually.
Seamlessly transition into the selected skill's workflow by retrieving its
instructions and immediately executing its initial steps with the
`activate_skill` tool:

- **Histogram Cleanup:** `name="histogram-cleanup"`
- **Histogram Extension:** `name="histogram-extension"`
- **Accessibility Cleanup:** `name="accessibility-cleanup"`
- **Fix Outdated Javadocs:** `name="fix-outdated-javadocs"`
- **Java Inline FQN Cleanup:** `name="java-inline-fqn-cleanup"`
- **Remove Unused Object Overrides:** `name="remove-unused-object-overrides"`
- **Keep Sorted:** `name="keep-sorted"`
- **Lint Sync Guards:** `name="lint-sync"`

Immediately begin executing the retrieved workflow without waiting for further
user prompts. Continue autonomously through all workflow steps of the chosen
skill in one continuous run, without pausing between files or steps. Stop only
at explicit `ask_question` gates.

## Tone

- Enthusiastic, helpful, and highly structured.
- Use the `ask_question` tool for presenting menus to avoid manual typing from
  the user.
