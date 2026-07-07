# Shared Automated Review Protocol

Use these instructions to perform a final patch review for any Code Health
cleanup task.

## Review Prompt

> You are a highly experienced code reviewer specializing in Git patches for
> Code Health in Chromium. Your task is to analyze the provided git patch and
> provide comprehensive, constructive feedback.
>
> # Step by Step Instructions
>
> 1. Run `git diff HEAD` to generate the patch. Read the patch carefully.
>
> 2. Analyze the patch for potential issues across these specific areas:
>
>    - **Functionality & Verification (CRITICAL):** Does the code still work as
>      intended? Ensure any unused methods, variables, or imports resulting from
>      the change are also removed. Check callers, headers, and tests for
>      completeness.
>    - **Consistency & Style:** Are there any inconsistencies with existing code
>      or patterns? You MUST read and verify compliance with the applicable
>      language style guides: - **Java:**
>      [Google Java Style Guide](https://google.github.io/styleguide/javaguide.html)
>      and
>      [Chromium Java Style Guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/java/java.md)
>      \- **C++:**
>      [Chromium C++ Style Guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md)
>    - **Specific Cleanup Rules:** (The active skill will provide these rules.
>      You MUST evaluate the patch against them).
>    - **Dependencies:** Verify that no changes to the `deps` of the modified
>      target in `BUILD.gn` files are needed.
>
> 3. Formulate concise and constructive feedback for each identified issue.
>    Categorize findings by severity ([Critical], [Major], [Minor]). Provide a
>    clear "Why" and a **numbered list of specific steps** for "Suggested
>    Remediation" for each point.
>
> 4. If all criteria are met and the independent verification confirms no
>    issues, output exactly `PASS`. Otherwise, output the complete review.

## Handling Findings

If the review returns findings:

1. **Output Findings:** Display the complete review findings to the user.
2. Analyze the `[Critical]` and `[Major]` issues.
3. Apply the **Suggested Remediation** for each of these issues **sequentially
   (one by one)**.
4. Once all remediations have been applied, re-run the review protocol.
5. Repeat until the review returns `PASS`.
6. **Final Validation:** Once `PASS` is achieved, run `git cl format`. If a
   histogram was modified, also execute
   `python3 tools/metrics/histograms/validate_format.py`.
