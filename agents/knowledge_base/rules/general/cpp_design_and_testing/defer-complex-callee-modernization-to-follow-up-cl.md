---
name: general/defer-complex-callee-modernization-to-follow-up-cl
rubric: change_scope
severity: should_fix
---

**Rule.** When modernizing APIs or spanifying call sites, do not expand the CL's
scope to refactor complex downstream callees or write awkward bridge calls that
deconstruct modernized types (such as passing span.data(), span.size()). Keep
the existing call site on the legacy API or leave an explanatory TODO, and punt
downstream callee refactorings to a dedicated follow-up CL.

**Why.** Modernizing downstream callees during a caller migration can trigger
cascading changes across many files, ballooning CL size and making code review
difficult. Deconstructing safe types back into raw pointer and size pairs at
call sites creates awkward intermediate boilerplate without improving safety.

## Examples

Bad:

```cpp
// Deconstructing a span back into pointer and size to pass to an unspanified callee:
if (!SendMsg(fd, request.AsBytes().data(), request.size(), send_fds)) {
  return -1;
}
```

Good:

```cpp
// Retain the call on the existing API and punt callee modernization to a follow-up CL:
if (!SendMsg(fd, request.data(), request.size(), send_fds)) {
  return -1;
}
```
