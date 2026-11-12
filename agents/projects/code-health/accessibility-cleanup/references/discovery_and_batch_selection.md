# Discovery & Batch Selection

Follow this workflow to identify candidates to process.

## Steps

1. **Discovery:** Delegate to a **`self`** subagent (`invoke_subagent`,
   `TypeName="self"`, `Role="Accessibility Candidate Triage"`) with this exact
   prompt (or, if subagents are unavailable, follow these instructions inline):

   > "Run the discovery script `find_candidates.py` located in the skill's
   > `scripts/` directory:
   >
   > ```bash
   > python3 agents/projects/code-health/accessibility-cleanup/scripts/find_candidates.py
   > ```
   >
   > For the candidates returned by the script, triage them one-by-one (by
   > reading the file contents) to determine if they represent a genuine
   > accessibility issue as described in `references/patterns.md`, or compare
   > against general known common accessibility issues/mistakes. Skip any
   > candidates that are false positives (e.g., correct API contracts or already
   > using proper delegates).
   >
   > If a candidate represents a genuine anti-pattern, return its details.
   >
   > If all candidates from the script are false positives, or if the script
   > returns 'No candidates found', perform an open-ended codebase search using
   > code search tools to find other accessibility anti-patterns (e.g.,
   > searching for suspect uses of `announceForAccessibility`,
   > `setAccessibilityLiveRegion` with `assertive`, custom
   > `AccessibilityActionCompat` label overrides, or proactive `requestFocus`
   > focus jumps).
   >
   > Return details for the first valid candidate found (File Path, matching
   > lines, and a short explanation of why it is an issue)."

2. **Report Candidate & Proceed:** Report the identified candidate to the user
   with exactly this message format (replace the bracketed details with the
   findings): "I've identified a candidate for accessibility cleanup:

   - **File:** [File Path]
   - **Issue Lines:** [Issue Lines]
   - **Reason:** [Brief Explanation]"

   Then proceed directly to **Refactoring & Implementation** without asking the
   user for approval.
