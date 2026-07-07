# Automated Review Protocol: Histogram Cleanup

Use this prompt when delegating a final review of a histogram cleanup patch to
the **`generalist`** sub-agent.

## Review Prompt

Delegate the review to the **`generalist`** sub-agent with this exact prompt:

> "Follow the **Shared Automated Review Protocol** in
> `../../hub/references/automated_review.md`.
>
> In addition to the generic checks, you MUST evaluate the patch against these
> **Specific Cleanup Rules** for Histogram Cleanup:
>
> - **Orphaned References:** Verify that there are no leftover references to the
>   removed histogram in code or comments. Use `rg` or `cs` to verify.
> - **Legacy Comments:** Ensure that legacy comments referencing the histogram
>   have been cleaned up appropriately. "
