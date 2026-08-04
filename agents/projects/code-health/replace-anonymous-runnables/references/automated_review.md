# Automated Review Protocol: Replace Anonymous Runnables

Use this prompt when delegating a final review of a
`replace-anonymous-runnables` patch to the **`generalist`** sub-agent.

## Review Prompt

Delegate the review to the **`generalist`** sub-agent with this exact prompt:

> "Follow the **Shared Automated Review Protocol** in
> `../../hub/references/automated_review.md`.
>
> In addition to the generic checks, you MUST evaluate the patch against these
> **Specific Cleanup Rules** for Replace Anonymous Runnables:
>
> - **Rule 1: No functional changes.** The conversion to lambda must not change
>   the behavior.
> - **Rule 2: No annotations on overridden methods.** Verify that none of the
>   converted anonymous classes had annotations (like `@JavascriptInterface` or
>   `@SuppressLint`) on their methods. Lambdas cannot have annotations on the
>   implemented method, and moving them (e.g. `@SuppressLint`) to the field or
>   enclosing method to bypass this is not allowed.
> - **Rule 3: Correct `this` reference.** Ensure that the original anonymous
>   class did not use `this` to refer to its own instance. If it did, the
>   conversion to lambda is incorrect because `this` now refers to the enclosing
>   class.
> - **Rule 4: No blank final field references in field initializers.** Verify
>   that the converted anonymous class was not in a field initializer and
>   referencing blank final fields initialized in the constructor (which causes
>   compile errors).
> - **Rule 5: Standard Formatting.** Ensure the lambdas are formatted correctly
>   according to Chromium Java style guide."
