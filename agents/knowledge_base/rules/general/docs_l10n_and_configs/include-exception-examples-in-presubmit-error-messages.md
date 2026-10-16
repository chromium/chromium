---
name: general/include-exception-examples-in-presubmit-error-messages
rubric: readability
severity: optional
---

**Rule.** When authoring or modifying presubmit checks that restrict or disallow
specific APIs, patterns, or headers in production code, include concrete
examples of legitimate exceptions or valid use cases in the diagnostic message.
Do not emit bare prohibition messages directing developers to contact owners
without providing context on what scenarios qualify for an exemption.

**Why.** Clear presubmit error and warning messages with concrete exception
examples help developers evaluate whether their specific usage (such as native
OS API integration) warrants an exemption before escalating to directory owners,
reducing review churn and confusion.

## Examples

Bad:

```cpp
output_api.PresubmitError(
    'base::RunLoop is disallowed in production code in //chrome. '
    'Please email chrome-directory-owners@chromium.org if you '
    'believe a RunLoop is necessary.',
    items=problems)
```

Good:

```cpp
output_api.PresubmitError(
    'base::RunLoop is disallowed in production code in //chrome. '
    'Please email chrome-directory-owners@chromium.org if you '
    'believe a RunLoop is necessary. It could be necessary for '
    'integration with OS APIs.',
    items=problems)
```
