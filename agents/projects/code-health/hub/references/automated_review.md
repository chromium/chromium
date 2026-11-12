# Shared Automated Review Protocol

## Overview

Unless the active skill explicitly opts out (e.g., data-only or
documentation-only tasks), the executing agent spawns an independent `self`
subagent to review the git diff. The reviewer returns a structured JSON verdict
that the executing agent acts on.

## Invocation Protocol

Invoke the reviewer with exactly these parameters:

- **Tool:** `invoke_subagent`
- **`TypeName`:** `"self"`
- **`Role`:** `"Code Reviewer"`
- **`Workspace`:** `"inherit"`

Pass the **Subagent Review Prompt Template** below as the prompt, replacing
`<SKILL_SPECIFIC_CLEANUP_RULES>` as described next.

If the active skill has a `references/review_rules.md` file, the executing agent
**MUST** read it and inject its contents into `<SKILL_SPECIFIC_CLEANUP_RULES>`.
If the skill relies solely on the hub's general criteria and has no
`review_rules.md`, replace `<SKILL_SPECIFIC_CLEANUP_RULES>` with
`None (evaluate against generic hub criteria)`.

## Subagent Review Prompt Template

> Act as a highly experienced code reviewer specializing in Git patches for Code
> Health in Chromium. Your task is to analyze the current change and report
> findings in a strict JSON format.
>
> # Step by Step Instructions
>
> 1. Run `git diff HEAD` to generate the patch. Read the patch carefully.
>
> 2. Analyze the patch for potential issues across these specific areas:
>
>    - **Functionality & Verification (CRITICAL):** Does the code still work as
>      intended? Check for regressions. Ensure no orphaned helpers, variables,
>      or imports remain as a result of the change. Check callers, headers, and
>      tests for completeness.
>    - **Dependencies:** Verify that no changes to the `deps` of the modified
>      target in `BUILD.gn` files are needed.
>    - **Dead Code, Leftover State & Comments (CRITICAL):** Ensure the change
>      left no orphaned helpers, unused variables or timers (e.g.,
>      `base::ElapsedTimer`, `SystemClock.elapsedRealtime()`), dead
>      constants/strings, unused imports/headers, or stale comments.
>    - **Style & Consistency:** Are there inconsistencies with existing code or
>      patterns? The applicable language style guide MUST be read and complied
>      with:
>      - **Java:**
>        [Google Java Style Guide](https://google.github.io/styleguide/javaguide.html)
>        and
>        [Chromium Java Style Guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/java/java.md)
>      - **C++:**
>        [Chromium C++ Style Guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md)
>    - **Specific Cleanup Rules:** The patch MUST be evaluated against the
>      following rules: `<SKILL_SPECIFIC_CLEANUP_RULES>`
>
> 3. Output your final verdict as a strict JSON block and nothing else:
>
>    ```json
>    {
>      "verdict": "PASS" | "FAIL",
>      "rules_evaluated": [
>        "exact rule or check evaluated 1",
>        "exact rule or check evaluated 2"
>      ],
>      "findings": [
>        {
>          "severity": "CRITICAL" | "MAJOR" | "MINOR",
>          "file": "path/to/file",
>          "message": "clear explanation of the issue",
>          "suggested_fix": "exact remediation steps"
>        }
>      ]
>    }
>    ```
>
>    Use `"verdict": "PASS"` whenever there are no `CRITICAL` or `MAJOR`
>    findings (non-blocking `MINOR` suggestions may still appear in `findings`).
>    Return `"verdict": "FAIL"` only when `CRITICAL` or `MAJOR` issues remain.

## Handling Findings & Iteration Cap

Parse the JSON block returned by the subagent.

Confirm that `rules_evaluated` contains every rule from the skill's
`references/review_rules.md` (when present) as well as the generic hub criteria
(Functionality & Verification, Dependencies, Dead Code, Leftover State &
Comments, and Style & Consistency). If any rule or criterion was omitted from
evaluation, the review is incomplete. Do not accept an incomplete `PASS`.
Instead, re-invoke the review subagent using the same protocol, explicitly
naming the omitted rules that it must evaluate (this is still subject to the
3-attempt maximum below). If the review is still incomplete after 3 attempts,
stop and escalate to the user.

- **`verdict == "PASS"`:** Display any `MINOR` findings if present, then proceed
  directly to Verification. No extra formatting run is needed because the Format
  & Lint check and every `FAIL` remediation round below already ran
  `format_and_lint.py`.
- **`verdict == "FAIL"`:**
  1. Display the findings to the user.
  2. Apply the `suggested_fix` for each `CRITICAL` and `MAJOR` finding
     sequentially (one by one).
  3. Run `python3 agents/projects/code-health/hub/scripts/format_and_lint.py`
     (which runs `git cl format` and, when histogram XML files changed,
     `vpython3 tools/metrics/histograms/validate_format.py`) so the fixes remain
     cleanly formatted and linted. If `format_and_lint.py` exits non-zero,
     resolve the reported formatting or validation errors before re-invoking the
     reviewer.
  4. Re-invoke the reviewer subagent using the same protocol.
  5. **Iteration Cap (Max 3):** Re-review at most **3 iterations**. If issues
     remain after 3 iterations, stop and escalate the remaining findings to the
     user.
