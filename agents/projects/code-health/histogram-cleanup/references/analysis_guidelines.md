# Histogram Cleanup Analysis Guidelines

Use these guidelines to evaluate the difficulty and safety of removing an
expired histogram.

## Safety Checks (Critical)

- **Test Dependencies:** Never remove a histogram if a test uses it as a primary
  signal for feature correctness (beyond just verifying the metric itself).
  Check for `HistogramTester` usage.
- **Shared Enums:** If a histogram is removed, check if its associated `<enum>`
  is used by other histograms before proposing its deletion from `enums.xml`.
- **Intentionally Expired (Diagnostics):** Do not clean up histograms marked
  with the `<expired_intentionally>` tag in the XML. This tag means the team
  specifically wants to keep the code around for local diagnostics. Only target
  expired histograms that *lack* this tag (forgotten technical debt).

## Effort Level Estimation (Guidelines)

When presenting candidates, provide an estimated effort level based on the
following:

- **🟢 Easy:**
  - 1-2 recording sites in the same file or component.
  - No tests or simple unit tests (e.g., using `HistogramTester`).
  - No XML `<variants>` or `<token>` expansions.
- **🟡 Medium:**
  - 3-5 recording sites or changes across 2-3 files.
  - Involves `browser_tests` or more complex mocks/simulations.
  - Contains simple `<variants>` expansions.
- **🔴 Hard:**
  - 6+ recording sites or logic spread across multiple layers (e.g., Renderer
    and Browser).
  - Involves `interactive_ui_tests` or complex multi-process state.
  - Heavy use of nested `<token>` expansions or shared enums that need cleaning.
