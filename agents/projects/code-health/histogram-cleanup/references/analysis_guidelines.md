# Histogram Cleanup Analysis Guidelines

Use these guidelines to evaluate the difficulty and safety of removing an
expired histogram.

## Safety Checks (Critical)

- **Test Dependencies:** Never remove a histogram if a test uses it as a primary
  signal for feature correctness (beyond just verifying the metric itself).
  Simple usages of `HistogramTester` to verify the metric was recorded are safe
  to remove and should NOT lower the Confidence Score.
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

## Generalist Deep Dive Prompt

When instructed to perform a deep dive on an expired histogram, use the
following exact workflow for the histogram `<HistogramName>`:

1. **Search:** Find ALL occurrences of this histogram string (including any
   expanded `<token>` or `<variants>` generated names) in the codebase.
   - **Fast Fail:** If your initial `cs` search returns a massive number of
     results (e.g., > 20) or immediately shows external references (e.g., Chrome
     OS `platform2`), STOP searching. Assign a Confidence Score of 0
     immediately.
   - You MUST use `cs` (CodeSearch) to check for external references (e.g.,
     Chrome OS `platform2` or internal repos). If external references exist, the
     removal is UNSAFE.
   - **Multi-line & Split Strings:** Be aware that in C++, histogram names are
     often split across multiple lines (e.g., `"My.Hist"` on one line and
     `"ogram.Name"` on the next). Search for chunks of the name rather than just
     the full string or the last dot-separated segment to ensure you find all
     call sites.
   - **Dot-less & Constants:** Search for the name with dots removed (e.g.,
     `MyExpiredHistogram`) to catch occurrences in constant names, variable
     identifiers, or Java resource IDs.
   - Use `rg` (ripgrep) for fast local discovery of the files you will need to
     edit.
2. **Safety Verification:** Strictly follow the 'Safety Checks' section in this
   document to identify test dependencies, shared enums, intentional expiry
   tags, and cross-repo dependencies. If the histogram has recording sites in
   external repositories, its Confidence Score MUST be 0.
3. **Scoring:** Based on your findings and the guidelines, calculate a
   Confidence Score (1-10) for its safe removal. (10/10 = 1-2 places, no complex
   test dependencies (HistogramTester is fine); < 7/10 = multiple sites, complex
   mocks; 0/10 = external repository dependencies).
4. **Removal Plan:** Formulate a concise plan for removal (e.g., 'Files to edit:
   X, Y; Entry to remove: Z').

**Return ONLY a concise summary of the affected files and tests, any identified
risks, the final Confidence Score, a brief justification, and the Removal
Plan.**
