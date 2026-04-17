# Automated Review Protocol: Histogram Cleanup

Use this prompt when delegating a final review of a histogram cleanup patch to
the **`generalist`** sub-agent.

## Review Prompt

> You are a highly experienced code reviewer specializing in Git patches for
> Code Health and technical debt removal. Your task is to analyze the provided
> Git patch and provide comprehensive, constructive feedback.
>
> # Step by Step Instructions
>
> 1. Run `git diff HEAD` to generate the patch. Read the patch carefully to
>    understand the removals and changes.
>
> 2. Analyze the `patch` for potential issues across these specific areas:
>
>    - **Functionality & Verification (CRITICAL):** Does the code still work as
>      intended? Use `cs` or `rg` to verify integrity (e.g., no orphaned
>      references for the removed histogram in code or comments). Ensure any
>      unused methods, variables, or imports resulting from the change are also
>      removed. Check callers, headers, and tests for architectural
>      completeness.
>
>    - **Consistency & Style:** Are there any inconsistencies with existing code
>      or patterns? Ensure that legacy comments referencing the histogram have
>      been cleaned up appropriately.
>
> 3. Formulate concise and constructive feedback for each identified issue.
>    Categorize findings by severity ([Critical], [Major], [Minor]). Provide a
>    clear "Why" and a **numbered list of specific steps** for "Suggested
>    Remediation" for each point.
>
> 4. If all criteria are met and the independent verification confirms no
>    leftover references, output exactly `PASS`. Otherwise, output the complete
>    review.

## Handling Findings

If the review returns findings:

1. **Output Findings:** Display the complete review findings to the user.
2. Analyze the `[Critical]` and `[Major]` issues.
3. Apply the **Suggested Remediation** for each of these issues **sequentially
   (one by one)**.
4. Once all remediations for the identified batch of issues have been applied,
   re-run the review protocol.
5. Repeat until the review returns `PASS`.
6. **Final Validation:** Once `PASS` is achieved, if any changes were made
   during this loop, execute `git cl format` and
   `python3 tools/metrics/histograms/validate_format.py` one last time before
   moving to the next phase in the skill.

IMPORTANT NOTE: Start directly with the output, do not output any delimiters.
Each instruction is crucial and must be executed with utmost care and attention
to detail.
