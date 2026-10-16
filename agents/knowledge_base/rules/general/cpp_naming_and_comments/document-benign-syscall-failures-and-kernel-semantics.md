---
name: general/document-benign-syscall-failures-and-kernel-semantics
rubric: readability
severity: nit
---

**Rule.** When invoking OS system calls or low-level platform APIs where
edge-case arguments, failure return values, or specific error codes (such as
`EINVAL` or `EPERM`) are ignored or treated as non-fatal, add an explanatory
code comment. Explain the expected OS/kernel behavior, version differences, or
error semantics, and clarify why the failure or edge case is benign.

**Why.** Low-level system calls and platform APIs frequently have complex,
platform-specific, or kernel-version-dependent behavior for edge-case inputs and
return codes. Without explanatory documentation, reviewers and future
maintainers cannot determine whether unhandled failure codes represent subtle
correctness bugs, sandbox issues, or intended no-op behavior.

## Examples

Bad:

```cpp
// If the call fails, it's not a correctness issue. However we want to catch
// the sandbox returning EPERM.
int retval =
    sched_setaffinity(thread_id.raw(), sizeof(*mask.mask), &*mask.mask);
DPCHECK(!retval);
```

Good:

```cpp
// If the call fails, it's not a correctness issue. However we want to catch
// the sandbox returning EPERM.
//
// For instance, an invalid mask (e.g. one with an empty intersection with the
// set of possible CPUs) returns EINVAL and does not change the current mask,
// per sched_setaffinity(2). On more recent kernels, an empty mask resets the
// affinity.
int retval =
    sched_setaffinity(thread_id.raw(), sizeof(*mask.mask), &*mask.mask);
DPCHECK(!retval);
```
