# Review Rules: Histogram Cleanup

- **Allowlisted Metadata Files (`safe_dirs.md`):** Every `histograms.xml`
  modified by the patch (`tools/metrics/histograms/metadata/*/histograms.xml`)
  MUST be listed in
  `agents/projects/code-health/histogram-cleanup/references/safe_dirs.md`. If
  any non-allowlisted `histograms.xml` was edited, return `FAIL`.
- **Shared Enums:** If an `<enum>` definition was removed from `enums.xml`,
  verify no remaining histogram in the codebase still references it, and verify
  the modified `enums.xml` is either next to an allowlisted `histograms.xml`
  (same `tools/metrics/histograms/metadata/<dir>/`) or the top-level
  `tools/metrics/histograms/enums.xml`; otherwise return `FAIL`.
- **Orphaned References:** Verify that there are no leftover references to the
  removed histogram in code or comments. Use `rg` or `cs` to verify.
- **Legacy Comments:** Ensure that legacy comments referencing the histogram
  have been cleaned up appropriately.
