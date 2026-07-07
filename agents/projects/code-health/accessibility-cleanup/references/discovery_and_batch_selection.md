# Discovery & Batch Selection

Follow this workflow to identify candidates to process.

## Steps

1. **Discovery:** Delegate to the **`generalist`** sub-agent with this exact
   prompt:

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
   > using proper delegates). Exclude any previously rejected files (if provided
   > by the user).
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

2. **Present Candidate & Triaging Loop:** Output the candidate details to the
   user. Announce the candidate with exactly this message format (replace the
   bracketed details with the findings): "I've identified a candidate for
   accessibility cleanup:

   - **File:** [File Path]
   - **Issue Lines:** [Issue Lines]
   - **Reason:** [Brief Explanation]"

   Ask the user for confirmation: "Shall I proceed with the cleanup of this
   file?"

   - **If the user agrees:** Proceed to **Refactoring & Implementation**.
   - **If the user rejects the candidate:** Loop back to Step 1 (AI-Led
     Discovery & Analysis), instructing the sub-agent to find the next candidate
     while explicitly excluding the rejected file path. Repeat this loop until a
     candidate is confirmed by the user.
