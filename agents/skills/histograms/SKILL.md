---
name: histograms
description: Manage Chromium histograms and enums across code and XML metadata. Use when adding, extending, revising/renaming, or removing/obsoleting histograms or enums in C++, Java, histograms.xml, or enums.xml.
---

# Histograms

This skill provides guidance and workflows for logging, extending, revising, and
obsoleting histograms and managing their metadata in the Chromium codebase.

## Overview

When adding, extending, revising, or removing code that logs a histogram, you
must keep the code, tests, `histograms.xml` / `enums.xml` metadata, and CL
description tags in sync so metrics are properly tracked and interpreted.

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
2. A relevant team (if possible). If you're unsure who to list as the second
   owner, ask the user.

### Expiry Date

Set `expires_after` to a date or milestone reasonable for how long the metric
will be used (typically 3–6 months for new metrics, up to a year; see
`//tools/metrics/histograms/README.md#guidelines-on-expiry`).

- **Extending expiry:** Only extend histograms that are still actively recorded
  and useful; remove histograms that are no longer needed. When extending a
  histogram that has already been expired for more than 1 month (or more than 1
  milestone), append a discontinuity warning to its `<summary>`
  (`//tools/metrics/histograms/README.md#extending`):
  `Warning: this histogram was expired from YYYY-MM-DD to YYYY-MM-DD; data may be missing.`
  (or the milestone equivalent).

### Revising or Obsoleting Histograms

- **Semantic changes:** Per `//tools/metrics/histograms/README.md#revising`, if
  a change alters when a histogram is emitted, what its buckets represent, or
  its bucket ranges, create a new histogram with a new name (typically by
  appending a version suffix like `2`), update its `<summary>`, and obsolete the
  old histogram.
- **Cleaning up code and XML:** Per
  `//tools/metrics/histograms/README.md#obsolete`, keep recording code and XML
  definitions in sync. When removing a histogram, remove all recording calls,
  related test assertions, the `<histogram>` entry in `histograms.xml`, and any
  now-unused `<variant>` or `<enum>` blocks. If an `<enum>` is still used, never
  delete or renumber individual values—mark deprecated values by prefixing their
  `label` with `(Obsolete) ` in `enums.xml`.
- **Obsoletion message in CL description:** When removing or renaming a
  histogram, include an unwrapped obsoletion tag on a single line in the CL
  description:
  - `OBSOLETE_HISTOGRAM[HistogramName]=Reason (e.g., Replaced by HistogramName2)`
  - `OBSOLETE_HISTOGRAMS=Shared reason for all histograms removed in this CL`

## Workflow

1. **Identify the Histogram:** Determine the name of the histogram being added,
   extended, revised, or removed.
2. **Locate Metadata Directory:** Find the appropriate subdirectory in
   `//tools/metrics/histograms/metadata/`.
3. **Update histograms.xml:** Add, update, or remove the `<histogram>` entry
   (and any associated `<variant>` entries).
4. **Update enums.xml (if applicable):** Ensure `<enum>` and `<int>` values in
   `enums.xml` match the C++/Java enum definitions, or remove/obsolete them as
   appropriate.
5. **Verify:** Run `python3 tools/metrics/histograms/validate_format.py` and
   `git cl format` to verify XML formatting and enum references, and ensure any
   required `OBSOLETE_HISTOGRAM` tags are present in the CL description. Check
   `//tools/metrics/histograms/README.md` for additional policies.
