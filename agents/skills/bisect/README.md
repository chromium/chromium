# Chromium Bisect Skill

This directory contains the **Bisect** skill for Chromium development.

## Overview

The `bisect` skill automates the process of finding the commit that introduced a
regression or broken build in the Chromium repository. It performs a
deterministic binary search over git commit ranges, manages dependency syncs
(`gclient sync -DR`), compiles targets using `autoninja`, and interactively
prompts the developer for test results at each step until the culprit commit is
isolated.

## Features

- **Deterministic Binary Search**: Calculates midpoints accurately and quickly
  using `git rev-list --reverse`.
- **Automated Sync & Build Workflow**: Handles `gclient sync -DR` and
  `autoninja -C <out_dir> <target>` at each iteration.
- **Configurable Environments**: Supports user-defined output directories (e.g.,
  `out/Default`, `out/Debug`) and build targets (e.g., `chrome_apk`, `chrome`,
  `content_shell_test_apk`).
- **Interactive Verification**: Queries the user after each build to test on
  their local device or simulator.
- **Comprehensive Culprit Report**: Generates a step-by-step bisect history
  table and extracts metadata (author, CL review link, commit position, bug
  link) for the culprit commit.

## Usage

### Slash Command / Direct Invocation

You can trigger a bisect using:

```text
/bisect <good_commit>..<bad_commit> , <out_dir> , <build_target>
```

Examples:

```text
/bisect a1b2c3d..e5f6g7h , out/Debug , chrome_apk
```

### Interactive Mode

If any arguments (commits, out directory, or target) are omitted, the agent will
prompt for them before beginning the bisect process.

## Files

- [SKILL.md](./SKILL.md): Main instructions, process flow, midpoint calculation
  script, and reporting specifications.
