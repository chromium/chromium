---
name: general/avoid-unverified-platform-doc-claims-in-commit-messages
rubric: readability
severity: should_fix
---

**Rule.** Do not include unverified claims about external or platform
documentation in commit message descriptions, nor infer API behaviors or error
code meanings from documentation of unrelated functions. Verify behavior and
error semantics against official specifications or testing for the specific API
being used before attributing discrepancies to upstream documentation.

**Why.** Inaccurate claims in commit messages misinform future maintainers,
obscure actual platform behaviors, and lead to incorrect assumptions during
debugging. Attribution of error codes or behavior to platform documentation
should be grounded in the specific API's verified contract rather than
inferences from separate APIs.

## Examples

Bad:

```cpp
Currently, `PIDForJob` logs an error message if called for a service that isn't registered, for which the error code is `ESRCH`. Apple's documentation incorrectly identifies this error as "The caller isn’t a process managed by launchd", so this log is confusing.
```

Good:

```cpp
Currently, `PIDForJob` logs an error message if called for a service that isn't registered, for which the error code is `ESRCH`.
```
