---
name: histograms
description: Manage Chromium histograms and enums metadata. Use this skill when adding code that logs a histogram or when updating metadata in histograms.xml or enums.xml.
---

# Histograms

This skill provides guidance and workflows for logging histograms and managing
their metadata in the Chromium codebase.

## Overview

When adding code that logs a histogram, you must also add metadata about that
histogram and any associated enums to ensure they are properly tracked and
interpreted.

## Guidelines

### Metadata Files

- **Histograms:** Metadata goes in `histograms.xml`.
- **Enums:** Metadata about enums goes in `enums.xml` in the same directory as
  the corresponding `histograms.xml`.

### Directory Structure

Metadata files are located in subdirectories of
`//tools/metrics/histograms/metadata/`. The subdirectory name is based on the
first part of the histogram name (e.g., `Platform.Foo` would likely be in
`metadata/platform/`). If the correct subdirectory is unclear, ask the user.

### Ownership

Histograms must have at least two owners:

1. The user who requested the change (usually the primary owner).
1. A relevant team (if possible). If you're unsure who to list as the second
   owner, ask the user.

### Expiry Date

Set the expiry date to 3 months in the future from the date you add the
histogram, unless a specific date is more appropriate.

## Workflow

1. **Identify the Histogram:** Determine the name of the histogram being logged
   or updated.
1. **Locate Metadata Directory:** Find the appropriate subdirectory in
   `//tools/metrics/histograms/metadata/`.
1. **Update histograms.xml:** Add or update the `<histogram>` entry.
1. **Update enums.xml (if applicable):** If the histogram uses an enum, ensure
   the corresponding `<enum>` and its `<int>` values are defined in `enums.xml`.
1. **Verify:** Check `//tools/metrics/histograms/README.md` for additional
   policies.
