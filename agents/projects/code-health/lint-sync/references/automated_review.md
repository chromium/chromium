# Automated Review Protocol: Lint Sync Guards

Use this prompt when delegating a final review of a lint-sync patch to the
**`generalist`** sub-agent.

## Review Prompt

Delegate the review to the **`generalist`** sub-agent with this exact prompt:

> "Follow the **Shared Automated Review Protocol** in
> `../../hub/references/automated_review.md`.
>
> In addition to the generic checks, you MUST evaluate the patch against these
> **Specific Cleanup Rules** for Lint Sync Guards:
>
> - **Strict Scope:** Focus ONLY on the LINT guards that were added in this
>   specific patch. Do NOT suggest adding guards to other pre-existing unguarded
>   enums in the file.
> - **Data Integrity:** If you identify missing entries in the XML, your
>   remediation MUST ONLY suggest appending them. Never suggest modifying or
>   removing existing names or values in either the source or the XML.
> - **Syntax & Placement:** Verify that the `LINT.IfChange` and
>   `LINT.ThenChange` guards are placed correctly before and after the enum
>   definitions in both the source code and the XML file. Verify that labels
>   match exactly and paths are valid (blank lines between the guards and the
>   code they protect are acceptable).
> - **Consistency & Style:** Ensure that legacy manual synchronization comments
>   (e.g., "keep in sync with enums.xml") have been removed, but all other
>   related comments have been preserved. "
